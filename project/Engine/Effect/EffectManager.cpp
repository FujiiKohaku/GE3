#include "EffectManager.h"

#include "Engine/Logger/Logger.h"
#include "Engine/Light/LightManager.h"
#include "Engine/Debug/DebugRenderer.h"
#include "Engine/StringUtility/StringUtility.h"
#include "Engine/TextureManager/TextureManager.h"
#include "Engine/math/MatrixMath.h"
#include "externals/json.hpp"

#include <cassert>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <format>

std::unique_ptr<EffectManager> EffectManager::instance_ = nullptr;

namespace {
constexpr const char* kEffectRoot = "resources/Effects";

std::filesystem::path ResolveEffectAssetPath(
    const std::filesystem::path& effectDirectory,
    const std::string& assetPath)
{
    std::filesystem::path path(assetPath);
    if (path.empty() || path.is_absolute()) {
        return path;
    }

    const std::string genericPath = path.generic_string();
    if (genericPath.starts_with("resources/")) {
        return path;
    }

    return effectDirectory / path;
}

ParticleMeshManager::ParticleMeshType ParseParticleMeshType(const std::string& meshType)
{
    if (meshType == "Ring") {
        return ParticleMeshManager::ParticleMeshType::Ring;
    }

    if (meshType == "Cylinder") {
        return ParticleMeshManager::ParticleMeshType::Cylinder;
    }

    if (meshType == "Box") {
        return ParticleMeshManager::ParticleMeshType::Box;
    }

    if (meshType == "Sphere") {
        return ParticleMeshManager::ParticleMeshType::Sphere;
    }

    if (meshType == "Cone") {
        return ParticleMeshManager::ParticleMeshType::Cone;
    }

    return ParticleMeshManager::ParticleMeshType::Board;
}

BlendMode ParseBlendMode(const std::string& blendMode)
{
    if (blendMode == "None") {
        return kBlendModeNone;
    }

    if (blendMode == "Normal") {
        return kBlendModeNormal;
    }

    if (blendMode == "Subtract") {
        return kBlendModeSubtract;
    }

    if (blendMode == "Multiply") {
        return kBlendModeMultiply;
    }

    if (blendMode == "Screen") {
        return kBlendModeScreen;
    }

    return kBlendModeAdd;
}

D3D12_CULL_MODE ParseCullMode(const std::string& cullMode)
{
    if (cullMode == "Back") {
        return D3D12_CULL_MODE_BACK;
    }

    if (cullMode == "Front") {
        return D3D12_CULL_MODE_FRONT;
    }

    return D3D12_CULL_MODE_NONE;
}

int32_t ParseEmitterShape(const std::string& shape)
{
    if (shape == "Box") {
        return 1;
    }

    if (shape == "Cone") {
        return 2;
    }

    if (shape == "Cylinder") {
        return 3;
    }

    if (shape == "Circle") {
        return 4;
    }

    return 0;
}

int32_t ParseParticleFieldType(const std::string& type)
{
    if (type == "Attractor") {
        return 1;
    }
    if (type == "Repulsor") {
        return 2;
    }
    if (type == "Vortex") {
        return 3;
    }
    return 0;
}

const nlohmann::json& SelectJsonSection(const nlohmann::json& config, const char* sectionName)
{
    if (config.contains(sectionName)) {
        return config.at(sectionName);
    }

    return config;
}

int32_t ReadBoolFlag(const nlohmann::json& config, const char* key, int32_t fallback)
{
    if (!config.contains(key)) {
        return fallback;
    }

    if (config.at(key).get<bool>()) {
        return 1;
    }

    return 0;
}

Vector3 ReadVector3(const nlohmann::json& json, const Vector3& fallback)
{
    if (!json.is_array() || json.size() < 3) {
        return fallback;
    }

    return {
        json.at(0).get<float>(),
        json.at(1).get<float>(),
        json.at(2).get<float>(),
    };
}

Vector4 ReadVector4(const nlohmann::json& json, const Vector4& fallback)
{
    if (!json.is_array() || json.size() < 4) {
        return fallback;
    }

    return {
        json.at(0).get<float>(),
        json.at(1).get<float>(),
        json.at(2).get<float>(),
        json.at(3).get<float>(),
    };
}
}

EffectManager::EffectManager(ConstructorKey)
{
}

EffectManager* EffectManager::GetInstance()
{
    if (!instance_) {
        instance_ = std::make_unique<EffectManager>(ConstructorKey());
    }
    return instance_.get();
}

void EffectManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, Camera* camera)
{
#ifdef _DEBUG
    const std::chrono::steady_clock::time_point initializeBeginTime =
        std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point phaseBeginTime = initializeBeginTime;
#endif

    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    camera_ = camera;

    particleRenderManager_ = std::make_unique<ParticleRenderManager>();
    particleRenderManager_->Initialize(dxCommon_);

#ifdef _DEBUG
    Logger::Log(std::format(
        "[EffectInit] ParticleRenderManager: {} ms",
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - phaseBeginTime).count()));
    phaseBeginTime = std::chrono::steady_clock::now();
#endif

    particleMeshManager_ = std::make_unique<ParticleMeshManager>();
    particleMeshManager_->Initialize(dxCommon_);

#ifdef _DEBUG
    Logger::Log(std::format(
        "[EffectInit] ParticleMeshManager: {} ms",
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - phaseBeginTime).count()));
    phaseBeginTime = std::chrono::steady_clock::now();
#endif

    CreateMaterialResource();
    CreatePerViewResource();

    CreateInitializeRootSignature();
    CreateInitializePipeline();
    CreateEmitRootSignature();
    CreateUpdateRootSignature();
    CreateFieldRootSignature();
    CreateFieldPipelines();

#ifdef _DEBUG
    Logger::Log(std::format(
        "[EffectInit] Common resources and root signatures: {} ms",
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - phaseBeginTime).count()));
    phaseBeginTime = std::chrono::steady_clock::now();
#endif

    RegisterDefaultEffects();

#ifdef _DEBUG
    Logger::Log(std::format(
        "[EffectInit] Register {} effects: {} ms",
        effects_.size(),
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - phaseBeginTime).count()));
    phaseBeginTime = std::chrono::steady_clock::now();
#endif

#ifdef _DEBUG
    Logger::Log(std::format(
        "[EffectInit] Total: {} ms",
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - initializeBeginTime).count()));
#endif
}

void EffectManager::RegisterDefaultEffects()
{
    const std::filesystem::path effectRoot(kEffectRoot);
    if (!std::filesystem::exists(effectRoot)) {
        return;
    }

    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(effectRoot)) {
        if (!entry.is_directory()) {
            continue;
        }

        const std::filesystem::path effectDirectory = entry.path();
        const std::string effectName = effectDirectory.filename().string();
        const std::filesystem::path jsonPath = effectDirectory / "Effect.json";
        const std::filesystem::path emitShaderPath = effectDirectory / "Emit.CS.hlsl";
        const std::filesystem::path updateShaderPath = effectDirectory / "Update.CS.hlsl";
        if (!std::filesystem::is_regular_file(jsonPath) ||
            !std::filesystem::is_regular_file(emitShaderPath) ||
            !std::filesystem::is_regular_file(updateShaderPath)) {
            continue;
        }

        RegisterEffect({
            effectName,
            effectDirectory.generic_string(),
            jsonPath.generic_string(),
            emitShaderPath.generic_string(),
            updateShaderPath.generic_string(),
        });
    }
}

void EffectManager::RegisterEffect(const EffectData& effectData)
{
    assert(!effectData.effectName.empty());
    effects_[effectData.effectName] = CreateEffectRuntime(effectData);
}

void EffectManager::BeginWarmUp()
{
    if (isWarmUpComplete_ || !warmUpEffectNames_.empty()) {
        return;
    }

    warmUpEffectNames_.reserve(effects_.size());
    for (const auto& [effectName, runtime] : effects_) {
        (void)runtime;
        warmUpEffectNames_.push_back(effectName);
    }
    warmUpEffectIndex_ = 0;
    isWarmUpComplete_ = warmUpEffectNames_.empty();
}

void EffectManager::UpdateWarmUp()
{
    if (isWarmUpComplete_) {
        return;
    }
    if (warmUpEffectNames_.empty()) {
        BeginWarmUp();
    }
    if (warmUpEffectIndex_ >= warmUpEffectNames_.size()) {
        isWarmUpComplete_ = true;
        return;
    }

    const std::string& effectName = warmUpEffectNames_[warmUpEffectIndex_];
    if (!WarmUpEffect(effectName)) {
        Logger::Log(std::format("[EffectWarmUp] Failed: {}", effectName));
    }
    ++warmUpEffectIndex_;

    if (warmUpEffectIndex_ >= warmUpEffectNames_.size()) {
        isWarmUpComplete_ = true;
        Logger::Log(std::format("[EffectWarmUp] Completed: {} effects", warmUpEffectNames_.size()));
    }
}

bool EffectManager::IsWarmUpComplete() const
{
    return isWarmUpComplete_;
}

float EffectManager::GetWarmUpProgress() const
{
    if (isWarmUpComplete_) {
        return 1.0f;
    }
    if (warmUpEffectNames_.empty()) {
        return 0.0f;
    }
    return static_cast<float>(warmUpEffectIndex_) / static_cast<float>(warmUpEffectNames_.size());
}

bool EffectManager::WarmUpEffect(const std::string& effectName)
{
    const Vector3 warmUpPosition = { 0.0f, -10000.0f, 0.0f };
    const float warmUpDuration = deltaTime_ * 3.0f;
    const EffectHandle handle = StartEffect(effectName, warmUpPosition, false, warmUpDuration, nullptr);
    if (handle == kInvalidEffectHandle) {
        return false;
    }

    const size_t activeEffectIndex = FindActiveEffectIndex(handle);
    if (activeEffectIndex == static_cast<size_t>(-1)) {
        return false;
    }

    UpdateActiveEffect(activeEffectIndex);
    return true;
}

EffectManager::EffectRuntime EffectManager::CreateEffectRuntime(const EffectData& effectData)
{
    EffectRuntime runtime {};
    runtime.data = effectData;
    if (!runtime.data.vertexShaderPath.empty()) {
        runtime.vertexShaderPath = runtime.data.vertexShaderPath;
    }
    if (!runtime.data.pixelShaderPath.empty()) {
        runtime.pixelShaderPath = runtime.data.pixelShaderPath;
    }

    ApplyEffectConfig(effectData, runtime);
    if (runtime.renderType == ParticleRenderType::Mesh) {
        runtime.emitPipelineState = CreateComputePipeline(
            runtime.data.effectName,
            "Emit",
            emitRootSignature_.Get(),
            runtime.data.emitShaderPath);
    }
    runtime.updatePipelineState = CreateComputePipeline(
        runtime.data.effectName,
        "Update",
        updateRootSignature_.Get(),
        runtime.data.updateShaderPath);

    ParticleRenderManager::GraphicsPipelineDesc graphicsPipelineDesc {};
    graphicsPipelineDesc.effectName = runtime.data.effectName;
    graphicsPipelineDesc.vertexShaderPath = runtime.vertexShaderPath;
    graphicsPipelineDesc.pixelShaderPath = runtime.pixelShaderPath;
    graphicsPipelineDesc.blendMode = runtime.blendMode;
    graphicsPipelineDesc.depthTest = runtime.depthTest;
    graphicsPipelineDesc.depthWrite = runtime.depthWrite;
    graphicsPipelineDesc.cullMode = runtime.cullMode;
    if (runtime.renderType == ParticleRenderType::Trail) {
        graphicsPipelineDesc.usesVertexInput = false;
    }
    runtime.graphicsPipelineState = particleRenderManager_->CreateGraphicsPipeline(graphicsPipelineDesc);

    TextureManager::GetInstance()->LoadTexture(runtime.texturePath);
    return runtime;
}

void EffectManager::ApplyEffectConfig(const EffectData& effectData, EffectRuntime& runtime)
{
    if (effectData.jsonPath.empty()) {
        return;
    }

    std::ifstream file(effectData.jsonPath);
    if (!file.is_open()) {
        assert(false);
        return;
    }

    nlohmann::json config;
    file >> config;

    const std::filesystem::path effectDirectory(effectData.effectDirectory);

    const nlohmann::json& emitter = SelectJsonSection(config, "Emitter");
    const nlohmann::json& particle = SelectJsonSection(config, "Particle");
    const nlohmann::json& simulation = SelectJsonSection(config, "Simulation");
    const nlohmann::json& render = SelectJsonSection(config, "Render");

    if (config.contains("Shaders")) {
        const nlohmann::json& shaders = config.at("Shaders");
        if (shaders.contains("Emit")) {
            runtime.data.emitShaderPath = ResolveEffectAssetPath(
                effectDirectory,
                shaders.at("Emit").get<std::string>())
                                             .generic_string();
        }

        if (shaders.contains("Update")) {
            runtime.data.updateShaderPath = ResolveEffectAssetPath(
                effectDirectory,
                shaders.at("Update").get<std::string>())
                                               .generic_string();
        }

        if (shaders.contains("Vertex")) {
            runtime.vertexShaderPath = ResolveEffectAssetPath(
                effectDirectory,
                shaders.at("Vertex").get<std::string>())
                                           .generic_string();
            runtime.data.vertexShaderPath = runtime.vertexShaderPath;
        }

        if (shaders.contains("Pixel")) {
            runtime.pixelShaderPath = ResolveEffectAssetPath(
                effectDirectory,
                shaders.at("Pixel").get<std::string>())
                                          .generic_string();
            runtime.data.pixelShaderPath = runtime.pixelShaderPath;
        }
    }

    if (emitter.contains("Shape")) {
        runtime.settings.emitterShape = ParseEmitterShape(emitter.at("Shape").get<std::string>());
    }

    runtime.emitCount = emitter.value("Count", emitter.value("EmitCount", runtime.emitCount));
    runtime.emitRadius = emitter.value("Radius", emitter.value("EmitRadius", runtime.emitRadius));
    runtime.emitFrequency = emitter.value("Frequency", emitter.value("EmitFrequency", runtime.emitFrequency));

    runtime.settings.lifeTime = particle.value("LifeTime", runtime.settings.lifeTime);
    runtime.settings.startScale = particle.value("StartScale", runtime.settings.startScale);
    runtime.settings.endScale = particle.value("EndScale", runtime.settings.endScale);
    runtime.settings.startRotation = particle.value("StartRotation", runtime.settings.startRotation);
    runtime.settings.rotationSpeed = particle.value("RotationSpeed", runtime.settings.rotationSpeed);

    if (particle.contains("Velocity")) {
        runtime.settings.velocity = ReadVector3(particle.at("Velocity"), runtime.settings.velocity);
    }

    if (particle.contains("StartColor")) {
        runtime.settings.startColor = ReadVector4(particle.at("StartColor"), runtime.settings.startColor);
    }

    if (particle.contains("EndColor")) {
        runtime.settings.endColor = ReadVector4(particle.at("EndColor"), runtime.settings.endColor);
    }

    runtime.settings.enableGravity = ReadBoolFlag(simulation, "EnableGravity", runtime.settings.enableGravity);
    runtime.settings.gravity = simulation.value("Gravity", runtime.settings.gravity);
    runtime.settings.enableDrag = ReadBoolFlag(simulation, "EnableDrag", runtime.settings.enableDrag);
    runtime.settings.drag = simulation.value("Drag", runtime.settings.drag);
    runtime.settings.enableNoise = ReadBoolFlag(simulation, "EnableNoise", runtime.settings.enableNoise);
    runtime.settings.noiseStrength = simulation.value("NoiseStrength", runtime.settings.noiseStrength);
    runtime.settings.enableAttraction = ReadBoolFlag(simulation, "EnableAttraction", runtime.settings.enableAttraction);
    runtime.settings.attractionStrength = simulation.value("AttractionStrength", runtime.settings.attractionStrength);

    if (render.contains("Texture")) {
        runtime.texturePath = ResolveEffectAssetPath(
            effectDirectory,
            render.at("Texture").get<std::string>())
                                  .generic_string();
    }

    if (render.contains("MeshType")) {
        runtime.meshType = ParseParticleMeshType(render.at("MeshType").get<std::string>());
    }

    if (render.contains("Type")) {
        const std::string renderType = render.at("Type").get<std::string>();
        if (renderType == "Trail") {
            runtime.renderType = ParticleRenderType::Trail;
        } else {
            runtime.renderType = ParticleRenderType::Mesh;
        }
    }

    if (render.contains("BlendMode")) {
        runtime.blendMode = ParseBlendMode(render.at("BlendMode").get<std::string>());
    }

    if (render.contains("DepthTest")) {
        runtime.depthTest = render.at("DepthTest").get<bool>();
    }

    if (render.contains("DepthWrite")) {
        runtime.depthWrite = render.at("DepthWrite").get<bool>();
    }

    if (render.contains("CullMode")) {
        runtime.cullMode = ParseCullMode(render.at("CullMode").get<std::string>());
    }

    if (config.contains("RenderParameter")) {
        ApplyRenderParameterConfig(config.at("RenderParameter"), runtime.renderParameter);
    }
    if (render.contains("RenderParameter")) {
        ApplyRenderParameterConfig(render.at("RenderParameter"), runtime.renderParameter);
    }
    ApplyRenderParameterConfig(render, runtime.renderParameter);

    if (config.contains("Trail")) {
        const nlohmann::json& trail = config.at("Trail");
        if (trail.contains("StartColor")) {
            runtime.renderParameter.trailStartColor = ReadVector4(
                trail.at("StartColor"),
                runtime.renderParameter.trailStartColor);
        }
        if (trail.contains("EndColor")) {
            runtime.renderParameter.trailEndColor = ReadVector4(
                trail.at("EndColor"),
                runtime.renderParameter.trailEndColor);
        }
        runtime.renderParameter.trailLifeTime = trail.value(
            "LifeTime",
            runtime.renderParameter.trailLifeTime);
        runtime.renderParameter.trailStartWidth = trail.value(
            "StartWidth",
            runtime.renderParameter.trailStartWidth);
        runtime.renderParameter.trailEndWidth = trail.value(
            "EndWidth",
            runtime.renderParameter.trailEndWidth);
        runtime.renderParameter.trailTextureTiling = trail.value(
            "TextureTiling",
            runtime.renderParameter.trailTextureTiling);
        runtime.renderParameter.trailMinVertexDistance = trail.value(
            "MinVertexDistance",
            runtime.renderParameter.trailMinVertexDistance);
        runtime.renderParameter.trailBreakDistance = trail.value(
            "BreakDistance",
            runtime.renderParameter.trailBreakDistance);
        runtime.renderParameter.maxTrailPoints = trail.value(
            "MaxPoints",
            runtime.renderParameter.maxTrailPoints);
        runtime.renderParameter.faceCamera = ReadBoolFlag(
            trail,
            "FaceCamera",
            runtime.renderParameter.faceCamera);
        runtime.renderParameter.trailRootExtension = trail.value(
            "RootExtension",
            runtime.renderParameter.trailRootExtension);

        if (runtime.renderParameter.maxTrailPoints < 2) {
            runtime.renderParameter.maxTrailPoints = 2;
        }
        if (runtime.renderParameter.maxTrailPoints > kMaxTrailPoints) {
            runtime.renderParameter.maxTrailPoints = kMaxTrailPoints;
        }
        if (runtime.renderParameter.trailLifeTime <= 0.0f) {
            runtime.renderParameter.trailLifeTime = 0.01f;
        }
        if (runtime.renderParameter.trailMinVertexDistance < 0.0f) {
            runtime.renderParameter.trailMinVertexDistance = 0.0f;
        }
        if (runtime.renderParameter.trailBreakDistance < runtime.renderParameter.trailMinVertexDistance) {
            runtime.renderParameter.trailBreakDistance = runtime.renderParameter.trailMinVertexDistance;
        }
        if (runtime.renderParameter.trailRootExtension < 0.0f) {
            runtime.renderParameter.trailRootExtension = 0.0f;
        }
    }

    if (config.contains("Light")) {
        const nlohmann::json& light = config.at("Light");
        runtime.lightSettings.enabled = ReadBoolFlag(
            light,
            "Enabled",
            runtime.lightSettings.enabled);

        if (light.contains("Type")) {
            const std::string lightType = light.at("Type").get<std::string>();
            if (lightType != "Point") {
                runtime.lightSettings.enabled = 0;
                Logger::Warning(
                    "Effect Light currently supports Point only. Effect:" +
                    effectData.effectName + " Type:" + lightType);
            }
        }

        if (light.contains("Color")) {
            runtime.lightSettings.color = ReadVector4(
                light.at("Color"),
                runtime.lightSettings.color);
        }
        if (light.contains("Offset")) {
            runtime.lightSettings.offset = ReadVector3(
                light.at("Offset"),
                runtime.lightSettings.offset);
        }

        runtime.lightSettings.intensity = light.value(
            "Intensity",
            runtime.lightSettings.intensity);
        runtime.lightSettings.radius = light.value(
            "Radius",
            runtime.lightSettings.radius);
        runtime.lightSettings.decay = light.value(
            "Decay",
            runtime.lightSettings.decay);
        runtime.lightSettings.fadeDuration = light.value(
            "FadeDuration",
            runtime.lightSettings.fadeDuration);
        runtime.lightSettings.followEmitter = ReadBoolFlag(
            light,
            "FollowEmitter",
            runtime.lightSettings.followEmitter);
        runtime.lightSettings.fadeOut = ReadBoolFlag(
            light,
            "FadeOut",
            runtime.lightSettings.fadeOut);

        if (runtime.lightSettings.intensity < 0.0f) {
            runtime.lightSettings.intensity = 0.0f;
        }
        if (runtime.lightSettings.radius < 0.01f) {
            runtime.lightSettings.radius = 0.01f;
        }
        if (runtime.lightSettings.decay < 0.0f) {
            runtime.lightSettings.decay = 0.0f;
        }
        if (runtime.lightSettings.fadeDuration < 0.0f) {
            runtime.lightSettings.fadeDuration = 0.0f;
        }
    }

    if (config.contains("Fields") && config.at("Fields").is_array()) {
        const nlohmann::json& fields = config.at("Fields");
        const size_t fieldLimit = fields.size();
        for (size_t fieldIndex = 0; fieldIndex < fieldLimit; ++fieldIndex) {
            if (runtime.fieldCount >= kMaxParticleFields) {
                Logger::Warning(
                    "Effect Field count exceeded maximum. Effect:" +
                    effectData.effectName);
                break;
            }

            const nlohmann::json& fieldJson = fields.at(fieldIndex);
            if (!fieldJson.is_object()) {
                continue;
            }

            ParticleFieldDefinition& definition =
                runtime.fields[runtime.fieldCount];
            if (fieldJson.contains("Type")) {
                definition.field.type = ParseParticleFieldType(
                    fieldJson.at("Type").get<std::string>());
            }
            if (fieldJson.contains("Position")) {
                definition.field.position = ReadVector3(
                    fieldJson.at("Position"),
                    definition.field.position);
            }
            if (fieldJson.contains("Direction")) {
                definition.field.direction = ReadVector3(
                    fieldJson.at("Direction"),
                    definition.field.direction);
            }
            if (fieldJson.contains("Space")) {
                const std::string fieldSpace =
                    fieldJson.at("Space").get<std::string>();
                if (fieldSpace == "World") {
                    definition.isLocal = 0;
                } else {
                    definition.isLocal = 1;
                }
            }

            definition.field.radius = fieldJson.value(
                "Radius",
                definition.field.radius);
            definition.field.strength = fieldJson.value(
                "Strength",
                definition.field.strength);
            definition.field.falloff = fieldJson.value(
                "Falloff",
                definition.field.falloff);

            if (definition.field.radius < 0.01f) {
                definition.field.radius = 0.01f;
            }
            if (definition.field.falloff < 0.01f) {
                definition.field.falloff = 0.01f;
            }

            runtime.fieldCount++;
        }
    }

    runtime.defaultLoop = render.value("Loop", runtime.defaultLoop);
    runtime.duration = render.value("Duration", runtime.duration);
}

void EffectManager::ApplyRenderParameterConfig(
    const nlohmann::json& config,
    ParticleRenderParameter& renderParameter)
{
    if (config.contains("DissolveThreshold")) {
        renderParameter.dissolveThreshold = config.at("DissolveThreshold").get<float>();
    }

    if (config.contains("EmissionStrength")) {
        renderParameter.emissionStrength = config.at("EmissionStrength").get<float>();
    }

    if (config.contains("UvScrollSpeedX")) {
        renderParameter.uvScrollSpeedX = config.at("UvScrollSpeedX").get<float>();
    }

    if (config.contains("UvScrollSpeedY")) {
        renderParameter.uvScrollSpeedY = config.at("UvScrollSpeedY").get<float>();
    }

    if (config.contains("UVScrollSpeedX")) {
        renderParameter.uvScrollSpeedX = config.at("UVScrollSpeedX").get<float>();
    }

    if (config.contains("UVScrollSpeedY")) {
        renderParameter.uvScrollSpeedY = config.at("UVScrollSpeedY").get<float>();
    }

    if (config.contains("UvScrollSpeed")) {
        const nlohmann::json& uvScrollSpeed = config.at("UvScrollSpeed");
        if (uvScrollSpeed.is_array() && uvScrollSpeed.size() >= 2) {
            renderParameter.uvScrollSpeedX = uvScrollSpeed.at(0).get<float>();
            renderParameter.uvScrollSpeedY = uvScrollSpeed.at(1).get<float>();
        }
    }

    if (config.contains("UVScrollSpeed")) {
        const nlohmann::json& uvScrollSpeed = config.at("UVScrollSpeed");
        if (uvScrollSpeed.is_array() && uvScrollSpeed.size() >= 2) {
            renderParameter.uvScrollSpeedX = uvScrollSpeed.at(0).get<float>();
            renderParameter.uvScrollSpeedY = uvScrollSpeed.at(1).get<float>();
        }
    }
}

Microsoft::WRL::ComPtr<ID3D12PipelineState> EffectManager::CreateComputePipeline(
    const std::string& effectName,
    const std::string& shaderStage,
    ID3D12RootSignature* rootSignature,
    const std::string& shaderPath)
{
#ifdef _DEBUG
    const std::chrono::steady_clock::time_point beginTime =
        std::chrono::steady_clock::now();
#endif

    const std::filesystem::path shaderFilePath(shaderPath);
    const std::filesystem::path fullShaderPath = std::filesystem::absolute(shaderFilePath);
    if (!std::filesystem::is_regular_file(shaderFilePath)) {
        Logger::Error(
            "Effect compute shader file is missing. Effect:" + effectName +
            " Stage:" + shaderStage +
            " Path:" + fullShaderPath.generic_string());
        assert(false);
        return nullptr;
    }

    Logger::Log(
        "Load effect compute shader. Effect:" + effectName +
        " Stage:" + shaderStage +
        " Path:" + fullShaderPath.generic_string());

    Microsoft::WRL::ComPtr<IDxcBlob> computeShaderBlob =
        dxCommon_->LoadCompiledShader(StringUtility::ConvertString(shaderPath));

    assert(computeShaderBlob);

    D3D12_COMPUTE_PIPELINE_STATE_DESC pipelineStateDesc {};
    pipelineStateDesc.pRootSignature = rootSignature;
    pipelineStateDesc.CS.pShaderBytecode = computeShaderBlob->GetBufferPointer();
    pipelineStateDesc.CS.BytecodeLength = computeShaderBlob->GetBufferSize();

    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
    HRESULT hr = dxCommon_->GetDevice()->CreateComputePipelineState(
        &pipelineStateDesc,
        IID_PPV_ARGS(&pipelineState));
    assert(SUCCEEDED(hr));

#ifdef _DEBUG
    Logger::Log(std::format(
        "[EffectPSO] {} {} Compute: {} ms",
        effectName,
        shaderStage,
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - beginTime).count()));
#endif

    return pipelineState;
}

EffectHandle EffectManager::PlayEffect(const std::string& effectName, const Vector3& position)
{
    return StartEffect(effectName, position, false, -1.0f, nullptr);
}

EffectHandle EffectManager::PlayLoopEffect(const std::string& effectName, const Vector3& position, float duration)
{
    return StartEffect(effectName, position, true, duration, nullptr);
}

EffectHandle EffectManager::AttachEffect(
    const std::string& effectName,
    EffectPositionProvider positionProvider,
    float duration)
{
    if (!positionProvider) {
        return kInvalidEffectHandle;
    }

    const Vector3 initialPosition = positionProvider();
    return StartEffect(effectName, initialPosition, true, duration, std::move(positionProvider));
}

EffectHandle EffectManager::StartEffect(
    const std::string& effectName,
    const Vector3& position,
    bool isLoop,
    float duration,
    EffectPositionProvider positionProvider)
{
    std::unordered_map<std::string, EffectRuntime>::const_iterator runtimeIterator = effects_.find(effectName);
    if (runtimeIterator == effects_.end()) {
        return kInvalidEffectHandle;
    }

    const EffectRuntime& runtime = runtimeIterator->second;
    if (!srvManager_->CanAllocate(4)) {
        return kInvalidEffectHandle;
    }

    const EffectHandle handle = AllocateEffectHandle();
    if (handle == kInvalidEffectHandle) {
        return kInvalidEffectHandle;
    }

    ActiveEffect activeEffect {};
    activeEffect.handle = handle;
    activeEffect.effectName = effectName;
    activeEffect.position = position;
    activeEffect.prevPosition = position;
    activeEffect.positionProvider = std::move(positionProvider);
    activeEffect.duration = duration;
    if (!isLoop && activeEffect.duration < 0.0f) {
        activeEffect.duration = runtime.duration;
    }
    activeEffect.isLoop = isLoop || runtime.defaultLoop;
    activeEffect.isAlive = true;

    ActiveEffectResource resource = CreateActiveEffectResource(runtime, position);
    UpdateEffectFields(runtime, activeEffect, resource);
    DispatchInitialize(resource);
    CreateEffectLight(runtime, activeEffect);

    activeEffects_.push_back(std::move(activeEffect));
    activeResources_.push_back(std::move(resource));

    return handle;
}

bool EffectManager::SetEffectPosition(EffectHandle handle, const Vector3& position)
{
    const size_t index = FindActiveEffectIndex(handle);
    if (index == static_cast<size_t>(-1)) {
        return false;
    }

    activeEffects_[index].positionProvider = nullptr;
    activeEffects_[index].position = position;
    return true;
}

bool EffectManager::SetEffectVelocity(EffectHandle handle, const Vector3& velocity)
{
    const size_t index = FindActiveEffectIndex(handle);
    if (index == static_cast<size_t>(-1)) {
        return false;
    }

    EffectSettings* settings = activeResources_[index].effectSettingsData;
    if (!settings) {
        return false;
    }

    settings->velocity = velocity;
    return true;
}

bool EffectManager::StopEffect(EffectHandle handle)
{
    const size_t index = FindActiveEffectIndex(handle);
    if (index == static_cast<size_t>(-1)) {
        return false;
    }

    std::unordered_map<std::string, EffectRuntime>::const_iterator runtimeIterator =
        effects_.find(activeEffects_[index].effectName);
    if (runtimeIterator != effects_.end()) {
        BeginEffectFadeOut(
            runtimeIterator->second,
            activeEffects_[index],
            activeResources_[index]);
        return true;
    }

    activeEffects_[index].isAlive = false;
    return true;
}

void EffectManager::StopAllEffects()
{
    for (ActiveEffect& activeEffect : activeEffects_) {
        ReleaseEffectLight(activeEffect);
    }

    if (activeResources_.empty() && retiredResources_.empty()) {
        activeEffects_.clear();
        return;
    }

    // 直前のフレームでGPUが参照している可能性があるため、解放前に完了を待つ。
    // EffectManager本体は破棄しないので、シェーダーやPSOは次のシーンでも再利用できる。
    if (dxCommon_ != nullptr) {
        dxCommon_->WaitForGPU();
    }

    for (ActiveEffectResource& resource : activeResources_) {
        UnmapActiveEffectResource(resource);
        ReleaseActiveEffectDescriptors(resource);
    }

    for (ActiveEffectResource& resource : retiredResources_) {
        UnmapActiveEffectResource(resource);
        ReleaseActiveEffectDescriptors(resource);
    }

    activeEffects_.clear();
    activeResources_.clear();
    retiredResources_.clear();
}

bool EffectManager::IsEffectAlive(EffectHandle handle) const
{
    return FindActiveEffectIndex(handle) != static_cast<size_t>(-1);
}

void EffectManager::CreateEffectLight(const EffectRuntime& runtime, ActiveEffect& activeEffect)
{
    if (runtime.lightSettings.enabled == 0) {
        return;
    }

    const Vector3 lightPosition = {
        activeEffect.position.x + runtime.lightSettings.offset.x,
        activeEffect.position.y + runtime.lightSettings.offset.y,
        activeEffect.position.z + runtime.lightSettings.offset.z,
    };

    const PointLightHandle lightHandle = LightManager::GetInstance()->AddPointLight(
        runtime.lightSettings.color,
        lightPosition,
        runtime.lightSettings.intensity,
        runtime.lightSettings.radius,
        runtime.lightSettings.decay);
    if (lightHandle == kInvalidPointLightHandle) {
        Logger::Warning(
            "Effect Point Light allocation failed. Effect:" +
            activeEffect.effectName);
        return;
    }

    activeEffect.pointLightHandle = lightHandle;
    activeEffect.lightBaseIntensity = runtime.lightSettings.intensity;
    activeEffect.lightFadeDuration = runtime.lightSettings.fadeDuration;
}

void EffectManager::UpdateEffectLight(
    const EffectRuntime& runtime,
    ActiveEffect& activeEffect,
    const ActiveEffectResource& resource)
{
    if (activeEffect.pointLightHandle == kInvalidPointLightHandle) {
        return;
    }

    LightManager* lightManager = LightManager::GetInstance();
    if (runtime.lightSettings.followEmitter != 0) {
        const Vector3 lightPosition = {
            activeEffect.position.x + runtime.lightSettings.offset.x,
            activeEffect.position.y + runtime.lightSettings.offset.y,
            activeEffect.position.z + runtime.lightSettings.offset.z,
        };
        lightManager->SetPointLightPosition(
            activeEffect.pointLightHandle,
            lightPosition);
    }

    float intensity = activeEffect.lightBaseIntensity;
    if (activeEffect.isLightFading) {
        if (activeEffect.lightFadeDuration <= 0.0f) {
            intensity = 0.0f;
        } else {
            const float fadeElapsed = resource.age - activeEffect.lightFadeStartAge;
            float fadeRate = fadeElapsed / activeEffect.lightFadeDuration;
            if (fadeRate < 0.0f) {
                fadeRate = 0.0f;
            }
            if (fadeRate > 1.0f) {
                fadeRate = 1.0f;
            }
            intensity = activeEffect.lightBaseIntensity * (1.0f - fadeRate);
        }
    }

    lightManager->SetPointLightIntensity(
        activeEffect.pointLightHandle,
        intensity);
}

void EffectManager::BeginEffectFadeOut(
    const EffectRuntime& runtime,
    ActiveEffect& activeEffect,
    const ActiveEffectResource& resource)
{
    if (!activeEffect.isAlive || !activeEffect.isEmitting) {
        return;
    }

    float tailDuration = 0.0f;
    if (runtime.renderType == ParticleRenderType::Trail) {
        tailDuration = runtime.renderParameter.trailLifeTime;
    }

    if (activeEffect.pointLightHandle != kInvalidPointLightHandle &&
        runtime.lightSettings.fadeOut != 0) {
        if (tailDuration < runtime.lightSettings.fadeDuration) {
            tailDuration = runtime.lightSettings.fadeDuration;
        }
        activeEffect.isLightFading = true;
        activeEffect.lightFadeStartAge = resource.age;
        activeEffect.lightFadeDuration = runtime.lightSettings.fadeDuration;
    }

    if (tailDuration <= 0.0f) {
        activeEffect.isAlive = false;
        return;
    }

    activeEffect.isEmitting = false;
    activeEffect.positionProvider = nullptr;
    activeEffect.duration = resource.age + tailDuration;
}

void EffectManager::ReleaseEffectLight(ActiveEffect& activeEffect)
{
    if (activeEffect.pointLightHandle == kInvalidPointLightHandle) {
        return;
    }

    LightManager::GetInstance()->RemovePointLight(activeEffect.pointLightHandle);
    activeEffect.pointLightHandle = kInvalidPointLightHandle;
}

void EffectManager::UpdateEffectFields(
    const EffectRuntime& runtime,
    const ActiveEffect& activeEffect,
    ActiveEffectResource& resource)
{
    if (!resource.fieldData) {
        return;
    }

    resource.fieldData->fieldCount = runtime.fieldCount;
    for (uint32_t fieldIndex = 0; fieldIndex < runtime.fieldCount; ++fieldIndex) {
        const ParticleFieldDefinition& definition = runtime.fields[fieldIndex];
        ParticleField field = definition.field;
        if (definition.isLocal != 0) {
            field.position.x += activeEffect.position.x;
            field.position.y += activeEffect.position.y;
            field.position.z += activeEffect.position.z;
        }
        resource.fieldData->fields[fieldIndex] = field;
    }
}

EffectManager::ActiveEffectResource EffectManager::CreateActiveEffectResource(
    const EffectRuntime& runtime,
    const Vector3& position)
{
    ActiveEffectResource resource {};
    resource.renderType = runtime.renderType;

    uint32_t particleCount = kMaxGPUParticle;
    uint32_t particleStride = sizeof(ParticleCS);
    const wchar_t* particleResourceName = L"EffectManager::ParticleBuffer";
    if (runtime.renderType == ParticleRenderType::Trail) {
        particleCount = kMaxTrailPoints;
        particleStride = sizeof(TrailPoint);
        particleResourceName = L"EffectManager::TrailPointBuffer";
    }

    resource.particleResource = CreateUavBufferResource(
        static_cast<size_t>(particleStride) * particleCount,
        particleResourceName);
    resource.freeListIndexResource = CreateUavBufferResource(sizeof(int32_t),L"EffectManager::FreeListIndex");
    resource.freeListResource = CreateUavBufferResource(sizeof(uint32_t) * kMaxGPUParticle,L"EffectManager::FreeList");

    resource.particleUavHandleGPU = CreateStructuredBufferUAV(
        resource.particleResource.Get(),
        particleCount,
        particleStride,
        resource.particleUavIndex);
    resource.particleSrvHandleGPU = CreateStructuredBufferSRV(
        resource.particleResource.Get(),
        particleCount,
        particleStride,
        resource.particleSrvIndex);
    resource.freeListIndexUavHandleGPU = CreateStructuredBufferUAV(
        resource.freeListIndexResource.Get(),
        1,
        sizeof(int32_t),
        resource.freeListIndexUavIndex);
    resource.freeListUavHandleGPU = CreateStructuredBufferUAV(
        resource.freeListResource.Get(),
        kMaxGPUParticle,
        sizeof(uint32_t),
        resource.freeListUavIndex);

    resource.emitterResource = dxCommon_->CreateBufferResource(sizeof(EmitterSphere));
    resource.emitterResource->SetName(L"EffectManager::EmitterSphere");
    resource.emitterResource->Map(0, nullptr, reinterpret_cast<void**>(&resource.emitterData));

    resource.emitterData->translate = position;
    resource.emitterData->radius = runtime.emitRadius;
    resource.emitterData->prevTranslate = position;
    resource.emitterData->padding1 = 0.0f;
    resource.emitterData->count = runtime.emitCount;
    resource.emitterData->frequency = runtime.emitFrequency;
    resource.emitterData->frequencyTime = 0.0f;
    resource.emitterData->emit = 0;

    resource.perFrameResource = dxCommon_->CreateBufferResource(sizeof(PerFrame));
    resource.perFrameResource->SetName(L"EffectManager::PerFrame");
    resource.perFrameResource->Map(0, nullptr, reinterpret_cast<void**>(&resource.perFrameData));
    resource.perFrameData->time = 0.0f;
    resource.perFrameData->deltaTime = deltaTime_;

    resource.effectSettingsResource = dxCommon_->CreateBufferResource(sizeof(EffectSettings));
    resource.effectSettingsResource->SetName(L"EffectManager::EffectSettings");
    resource.effectSettingsResource->Map(0, nullptr, reinterpret_cast<void**>(&resource.effectSettingsData));
    *resource.effectSettingsData = runtime.settings;

    resource.renderParameterResource = dxCommon_->CreateBufferResource(sizeof(ParticleRenderParameter));
    resource.renderParameterResource->SetName(L"EffectManager::ParticleRenderParameter");
    resource.renderParameterResource->Map(0, nullptr, reinterpret_cast<void**>(&resource.renderParameterData));
    *resource.renderParameterData = runtime.renderParameter;

    resource.fieldResource = dxCommon_->CreateBufferResource(sizeof(ParticleFieldCollection));
    resource.fieldResource->SetName(L"EffectManager::ParticleFieldCollection");
    resource.fieldResource->Map(0, nullptr, reinterpret_cast<void**>(&resource.fieldData));
    *resource.fieldData = {};

    return resource;
}

Microsoft::WRL::ComPtr<ID3D12Resource> EffectManager::CreateUavBufferResource(
    size_t sizeInBytes,
    const wchar_t* name)
{
    D3D12_RESOURCE_DESC resourceDesc {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Width = sizeInBytes;
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    D3D12_HEAP_PROPERTIES heapProperties {};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

    Microsoft::WRL::ComPtr<ID3D12Resource> resource;
    HRESULT hr = dxCommon_->GetDevice()->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(&resource));
    assert(SUCCEEDED(hr));

    resource->SetName(name);
    return resource;
}

D3D12_GPU_DESCRIPTOR_HANDLE EffectManager::CreateStructuredBufferUAV(
    ID3D12Resource* resource,
    uint32_t elementCount,
    uint32_t stride,
    uint32_t& descriptorIndex)
{
    descriptorIndex = srvManager_->Allocate();

    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc {};
    uavDesc.Format = DXGI_FORMAT_UNKNOWN;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.FirstElement = 0;
    uavDesc.Buffer.NumElements = elementCount;
    uavDesc.Buffer.StructureByteStride = stride;

    dxCommon_->GetDevice()->CreateUnorderedAccessView(
        resource,
        nullptr,
        &uavDesc,
        srvManager_->GetCPUDescriptorHandle(descriptorIndex));

    return srvManager_->GetGPUDescriptorHandle(descriptorIndex);
}

D3D12_GPU_DESCRIPTOR_HANDLE EffectManager::CreateStructuredBufferSRV(
    ID3D12Resource* resource,
    uint32_t elementCount,
    uint32_t stride,
    uint32_t& descriptorIndex)
{
    descriptorIndex = srvManager_->Allocate();

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc {};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements = elementCount;
    srvDesc.Buffer.StructureByteStride = stride;

    dxCommon_->GetDevice()->CreateShaderResourceView(
        resource,
        &srvDesc,
        srvManager_->GetCPUDescriptorHandle(descriptorIndex));

    return srvManager_->GetGPUDescriptorHandle(descriptorIndex);
}

void EffectManager::CreateMaterialResource()
{
    materialResource_ = dxCommon_->CreateBufferResource(sizeof(Material));
    materialResource_->SetName(L"EffectManager::MaterialCB");

    Material* materialBufferData = nullptr;
    materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialBufferData));

    materialData_.color = { 1.0f, 1.0f, 1.0f, 1.0f };
    materialData_.enableLighting = 0;
    materialData_.padding[0] = 0.0f;
    materialData_.padding[1] = 0.0f;
    materialData_.padding[2] = 0.0f;
    materialData_.uvTransform = MatrixMath::MakeIdentity4x4();
    materialData_.alphaReference = 0.1f;
    materialData_.padding2[0] = 0.0f;
    materialData_.padding2[1] = 0.0f;
    materialData_.padding2[2] = 0.0f;

    *materialBufferData = materialData_;

    materialResource_->Unmap(0, nullptr);
}

void EffectManager::CreatePerViewResource()
{
    perViewResource_ = dxCommon_->CreateBufferResource(sizeof(PerView));
    perViewResource_->SetName(L"EffectManager::PerViewCB");
    perViewResource_->Map(0, nullptr, reinterpret_cast<void**>(&perViewData_));

    perViewData_->viewProjection = MatrixMath::MakeIdentity4x4();
    perViewData_->billboardMatrix = MatrixMath::MakeIdentity4x4();
    perViewData_->cameraPosition = { 0.0f, 0.0f, 0.0f };
    perViewData_->padding = 0.0f;
}

void EffectManager::CreateInitializeRootSignature()
{
    D3D12_DESCRIPTOR_RANGE particleUavRange {};
    particleUavRange.BaseShaderRegister = 0;
    particleUavRange.NumDescriptors = 1;
    particleUavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    particleUavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_DESCRIPTOR_RANGE freeListIndexUavRange {};
    freeListIndexUavRange.BaseShaderRegister = 1;
    freeListIndexUavRange.NumDescriptors = 1;
    freeListIndexUavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    freeListIndexUavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_DESCRIPTOR_RANGE freeListUavRange {};
    freeListUavRange.BaseShaderRegister = 2;
    freeListUavRange.NumDescriptors = 1;
    freeListUavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    freeListUavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameters[3] {};
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[0].DescriptorTable.pDescriptorRanges = &particleUavRange;
    rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;

    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[1].DescriptorTable.pDescriptorRanges = &freeListIndexUavRange;
    rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;

    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[2].DescriptorTable.pDescriptorRanges = &freeListUavRange;
    rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;

    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc {};
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
    rootSignatureDesc.pParameters = rootParameters;
    rootSignatureDesc.NumParameters = _countof(rootParameters);

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    HRESULT hr = D3D12SerializeRootSignature(
        &rootSignatureDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &signatureBlob,
        &errorBlob);
    assert(SUCCEEDED(hr));

    hr = dxCommon_->GetDevice()->CreateRootSignature(
        0,
        signatureBlob->GetBufferPointer(),
        signatureBlob->GetBufferSize(),
        IID_PPV_ARGS(&initializeRootSignature_));
    assert(SUCCEEDED(hr));
}

void EffectManager::CreateInitializePipeline()
{
    initializePipelineState_ = CreateComputePipeline(
        "InitializeParticle",
        "Initialize",
        initializeRootSignature_.Get(),
        "resources/Shaders/Effects/Common/InitializeParticle.CS.hlsl");
    trailInitializePipelineState_ = CreateComputePipeline(
        "InitializeTrail",
        "Initialize",
        initializeRootSignature_.Get(),
        "resources/Shaders/Effects/Common/InitializeTrail.CS.hlsl");
}

void EffectManager::CreateEmitRootSignature()
{
    D3D12_DESCRIPTOR_RANGE particleUavRange {};
    particleUavRange.BaseShaderRegister = 0;
    particleUavRange.NumDescriptors = 1;
    particleUavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    particleUavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_DESCRIPTOR_RANGE freeListIndexUavRange {};
    freeListIndexUavRange.BaseShaderRegister = 1;
    freeListIndexUavRange.NumDescriptors = 1;
    freeListIndexUavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    freeListIndexUavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_DESCRIPTOR_RANGE freeListUavRange {};
    freeListUavRange.BaseShaderRegister = 2;
    freeListUavRange.NumDescriptors = 1;
    freeListUavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    freeListUavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameters[6] {};
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[0].Descriptor.ShaderRegister = 0;

    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[1].DescriptorTable.pDescriptorRanges = &particleUavRange;
    rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;

    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[2].DescriptorTable.pDescriptorRanges = &freeListIndexUavRange;
    rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;

    rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[3].DescriptorTable.pDescriptorRanges = &freeListUavRange;
    rootParameters[3].DescriptorTable.NumDescriptorRanges = 1;

    rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[4].Descriptor.ShaderRegister = 1;

    rootParameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[5].Descriptor.ShaderRegister = 2;

    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc {};
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
    rootSignatureDesc.pParameters = rootParameters;
    rootSignatureDesc.NumParameters = _countof(rootParameters);

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    HRESULT hr = D3D12SerializeRootSignature(
        &rootSignatureDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &signatureBlob,
        &errorBlob);
    assert(SUCCEEDED(hr));

    hr = dxCommon_->GetDevice()->CreateRootSignature(
        0,
        signatureBlob->GetBufferPointer(),
        signatureBlob->GetBufferSize(),
        IID_PPV_ARGS(&emitRootSignature_));
    assert(SUCCEEDED(hr));
}

void EffectManager::CreateUpdateRootSignature()
{
    D3D12_DESCRIPTOR_RANGE particleUavRange {};
    particleUavRange.BaseShaderRegister = 0;
    particleUavRange.NumDescriptors = 1;
    particleUavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    particleUavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_DESCRIPTOR_RANGE freeListIndexUavRange {};
    freeListIndexUavRange.BaseShaderRegister = 1;
    freeListIndexUavRange.NumDescriptors = 1;
    freeListIndexUavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    freeListIndexUavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_DESCRIPTOR_RANGE freeListUavRange {};
    freeListUavRange.BaseShaderRegister = 2;
    freeListUavRange.NumDescriptors = 1;
    freeListUavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    freeListUavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameters[6] {};
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[0].DescriptorTable.pDescriptorRanges = &particleUavRange;
    rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;

    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[1].DescriptorTable.pDescriptorRanges = &freeListIndexUavRange;
    rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;

    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[2].DescriptorTable.pDescriptorRanges = &freeListUavRange;
    rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;

    rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[3].Descriptor.ShaderRegister = 0;

    rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[4].Descriptor.ShaderRegister = 1;

    rootParameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[5].Descriptor.ShaderRegister = 2;

    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc {};
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
    rootSignatureDesc.pParameters = rootParameters;
    rootSignatureDesc.NumParameters = _countof(rootParameters);

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    HRESULT hr = D3D12SerializeRootSignature(
        &rootSignatureDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &signatureBlob,
        &errorBlob);
    assert(SUCCEEDED(hr));

    hr = dxCommon_->GetDevice()->CreateRootSignature(
        0,
        signatureBlob->GetBufferPointer(),
        signatureBlob->GetBufferSize(),
        IID_PPV_ARGS(&updateRootSignature_));
    assert(SUCCEEDED(hr));
}

void EffectManager::CreateFieldRootSignature()
{
    D3D12_DESCRIPTOR_RANGE particleUavRange {};
    particleUavRange.BaseShaderRegister = 0;
    particleUavRange.NumDescriptors = 1;
    particleUavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    particleUavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameters[3] {};
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[0].DescriptorTable.pDescriptorRanges = &particleUavRange;
    rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;

    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[1].Descriptor.ShaderRegister = 0;

    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[2].Descriptor.ShaderRegister = 1;

    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc {};
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
    rootSignatureDesc.pParameters = rootParameters;
    rootSignatureDesc.NumParameters = _countof(rootParameters);

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    HRESULT hr = D3D12SerializeRootSignature(
        &rootSignatureDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &signatureBlob,
        &errorBlob);
    assert(SUCCEEDED(hr));

    hr = dxCommon_->GetDevice()->CreateRootSignature(
        0,
        signatureBlob->GetBufferPointer(),
        signatureBlob->GetBufferSize(),
        IID_PPV_ARGS(&fieldRootSignature_));
    assert(SUCCEEDED(hr));
}

void EffectManager::CreateFieldPipelines()
{
    particleFieldPipelineState_ = CreateComputePipeline(
        "ParticleField",
        "Update",
        fieldRootSignature_.Get(),
        "resources/Shaders/Effects/Common/ApplyParticleFields.CS.hlsl");
    trailFieldPipelineState_ = CreateComputePipeline(
        "TrailField",
        "Update",
        fieldRootSignature_.Get(),
        "resources/Shaders/Effects/Common/ApplyTrailFields.CS.hlsl");
}

void EffectManager::DispatchInitialize(ActiveEffectResource& resource)
{
    TransitionResource(resource.particleResource.Get(), resource.particleState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
    ID3D12DescriptorHeap* descriptorHeaps[] = { srvManager_->GetDescriptorHeap() };
    commandList->SetDescriptorHeaps(1, descriptorHeaps);

    commandList->SetComputeRootSignature(initializeRootSignature_.Get());
    if (resource.renderType == ParticleRenderType::Trail) {
        commandList->SetPipelineState(trailInitializePipelineState_.Get());
        commandList->SetComputeRootDescriptorTable(0, resource.particleUavHandleGPU);
        commandList->Dispatch(1, 1, 1);
        InsertUavBarrier(resource.particleResource.Get());
        TransitionResource(resource.particleResource.Get(), resource.particleState, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        return;
    }

    TransitionResource(resource.freeListIndexResource.Get(), resource.freeListIndexState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    TransitionResource(resource.freeListResource.Get(), resource.freeListState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    commandList->SetPipelineState(initializePipelineState_.Get());
    commandList->SetComputeRootDescriptorTable(0, resource.particleUavHandleGPU);
    commandList->SetComputeRootDescriptorTable(1, resource.freeListIndexUavHandleGPU);
    commandList->SetComputeRootDescriptorTable(2, resource.freeListUavHandleGPU);
    commandList->Dispatch((kMaxGPUParticle + 255) / 256, 1, 1);

    InsertUavBarrier(resource.particleResource.Get());
    InsertUavBarrier(resource.freeListIndexResource.Get());
    InsertUavBarrier(resource.freeListResource.Get());
    TransitionResource(resource.particleResource.Get(), resource.particleState, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
}

void EffectManager::DispatchEmit(const EffectRuntime& runtime, ActiveEffectResource& resource)
{
    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
    ID3D12DescriptorHeap* descriptorHeaps[] = { srvManager_->GetDescriptorHeap() };
    commandList->SetDescriptorHeaps(1, descriptorHeaps);

    commandList->SetComputeRootSignature(emitRootSignature_.Get());
    commandList->SetPipelineState(runtime.emitPipelineState.Get());
    commandList->SetComputeRootConstantBufferView(0, resource.emitterResource->GetGPUVirtualAddress());
    commandList->SetComputeRootDescriptorTable(1, resource.particleUavHandleGPU);
    commandList->SetComputeRootDescriptorTable(2, resource.freeListIndexUavHandleGPU);
    commandList->SetComputeRootDescriptorTable(3, resource.freeListUavHandleGPU);
    commandList->SetComputeRootConstantBufferView(4, resource.perFrameResource->GetGPUVirtualAddress());
    commandList->SetComputeRootConstantBufferView(5, resource.effectSettingsResource->GetGPUVirtualAddress());

    const uint32_t dispatchCount = (resource.emitterData->count + 255) / 256;
    commandList->Dispatch(dispatchCount, 1, 1);
}

void EffectManager::DispatchUpdate(const EffectRuntime& runtime, ActiveEffectResource& resource)
{
    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
    ID3D12DescriptorHeap* descriptorHeaps[] = { srvManager_->GetDescriptorHeap() };
    commandList->SetDescriptorHeaps(1, descriptorHeaps);

    commandList->SetComputeRootSignature(updateRootSignature_.Get());
    commandList->SetPipelineState(runtime.updatePipelineState.Get());
    commandList->SetComputeRootDescriptorTable(0, resource.particleUavHandleGPU);
    if (runtime.renderType == ParticleRenderType::Trail) {
        commandList->SetComputeRootConstantBufferView(3, resource.perFrameResource->GetGPUVirtualAddress());
        commandList->SetComputeRootConstantBufferView(4, resource.renderParameterResource->GetGPUVirtualAddress());
        commandList->SetComputeRootConstantBufferView(5, resource.emitterResource->GetGPUVirtualAddress());
        commandList->Dispatch(1, 1, 1);
        return;
    }

    commandList->SetComputeRootDescriptorTable(1, resource.freeListIndexUavHandleGPU);
    commandList->SetComputeRootDescriptorTable(2, resource.freeListUavHandleGPU);
    commandList->SetComputeRootConstantBufferView(3, resource.perFrameResource->GetGPUVirtualAddress());
    commandList->SetComputeRootConstantBufferView(4, resource.effectSettingsResource->GetGPUVirtualAddress());
    commandList->SetComputeRootConstantBufferView(5, resource.emitterResource->GetGPUVirtualAddress());
    commandList->Dispatch((kMaxGPUParticle + 255) / 256, 1, 1);
}

void EffectManager::DispatchFields(const EffectRuntime& runtime, ActiveEffectResource& resource)
{
    if (!resource.fieldData || resource.fieldData->fieldCount == 0) {
        return;
    }

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
    ID3D12DescriptorHeap* descriptorHeaps[] = { srvManager_->GetDescriptorHeap() };
    commandList->SetDescriptorHeaps(1, descriptorHeaps);
    commandList->SetComputeRootSignature(fieldRootSignature_.Get());
    if (runtime.renderType == ParticleRenderType::Trail) {
        commandList->SetPipelineState(trailFieldPipelineState_.Get());
    } else {
        commandList->SetPipelineState(particleFieldPipelineState_.Get());
    }

    commandList->SetComputeRootDescriptorTable(0, resource.particleUavHandleGPU);
    commandList->SetComputeRootConstantBufferView(1, resource.fieldResource->GetGPUVirtualAddress());
    commandList->SetComputeRootConstantBufferView(2, resource.perFrameResource->GetGPUVirtualAddress());

    if (runtime.renderType == ParticleRenderType::Trail) {
        commandList->Dispatch(1, 1, 1);
    } else {
        commandList->Dispatch((kMaxGPUParticle + 255) / 256, 1, 1);
    }
}

void EffectManager::Update()
{
    ReleaseRetiredResources();

    for (size_t i = 0; i < activeEffects_.size(); ++i) {
        UpdateActiveEffect(i);
    }

    RemoveDeadEffects();
}

EffectHandle EffectManager::AllocateEffectHandle()
{
    for (uint32_t attempt = 0; attempt < kInvalidEffectHandle - 1; ++attempt) {
        const EffectHandle handle = nextEffectHandle_;
        nextEffectHandle_++;
        if (nextEffectHandle_ == kInvalidEffectHandle) {
            nextEffectHandle_ = 1;
        }

        if (FindActiveEffectIndex(handle) == static_cast<size_t>(-1)) {
            return handle;
        }
    }

    return kInvalidEffectHandle;
}

size_t EffectManager::FindActiveEffectIndex(EffectHandle handle) const
{
    if (handle == kInvalidEffectHandle) {
        return static_cast<size_t>(-1);
    }

    for (size_t i = 0; i < activeEffects_.size(); ++i) {
        if (activeEffects_[i].handle == handle && activeEffects_[i].isAlive) {
            return i;
        }
    }

    return static_cast<size_t>(-1);
}

void EffectManager::UpdateActiveEffect(size_t index)
{
    ActiveEffect& activeEffect = activeEffects_[index];
    ActiveEffectResource& resource = activeResources_[index];

    std::unordered_map<std::string, EffectRuntime>::const_iterator runtimeIterator =
        effects_.find(activeEffect.effectName);
    if (runtimeIterator == effects_.end()) {
        activeEffect.isAlive = false;
        return;
    }

    const EffectRuntime& runtime = runtimeIterator->second;

    activeEffect.prevPosition = activeEffect.position;

    if (activeEffect.positionProvider) {
        activeEffect.position = activeEffect.positionProvider();
    }

    resource.age += deltaTime_;
    resource.perFrameData->time = resource.age;
    resource.perFrameData->deltaTime = deltaTime_;
    UpdateEffectLight(runtime, activeEffect, resource);
    UpdateEffectFields(runtime, activeEffect, resource);

    resource.emitterData->translate = activeEffect.position;
    resource.emitterData->prevTranslate = activeEffect.prevPosition;
    resource.emitterData->radius = runtime.emitRadius;
    resource.emitterData->count = runtime.emitCount;
    resource.emitterData->frequency = runtime.emitFrequency;

    if (!activeEffect.isEmitting) {
        resource.emitterData->emit = 0;
    } else if (runtime.renderType == ParticleRenderType::Trail) {
        resource.emitterData->emit = 1;
        resource.hasEmitted = true;
    } else if (activeEffect.isLoop) {
        resource.emitterData->frequencyTime += deltaTime_;
        if (resource.emitterData->frequency <= resource.emitterData->frequencyTime) {
            resource.emitterData->frequencyTime -= resource.emitterData->frequency;
            resource.emitterData->emit = 1;
        } else {
            resource.emitterData->emit = 0;
        }
    } else if (!resource.hasEmitted) {
        resource.emitterData->emit = 1;
        resource.hasEmitted = true;
    } else {
        resource.emitterData->emit = 0;
    }

    TransitionResource(resource.particleResource.Get(), resource.particleState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    if (runtime.renderType == ParticleRenderType::Mesh && resource.emitterData->emit != 0) {
        DispatchEmit(runtime, resource);
        InsertUavBarrier(resource.particleResource.Get());
    }

    DispatchUpdate(runtime, resource);
    InsertUavBarrier(resource.particleResource.Get());

    if (resource.fieldData && resource.fieldData->fieldCount > 0) {
        DispatchFields(runtime, resource);
        InsertUavBarrier(resource.particleResource.Get());
    }

    TransitionResource(resource.particleResource.Get(), resource.particleState, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

    if (activeEffect.duration >= 0.0f && activeEffect.duration <= resource.age) {
        if (activeEffect.isEmitting) {
            BeginEffectFadeOut(runtime, activeEffect, resource);
        } else {
            activeEffect.isAlive = false;
        }
    }
}

void EffectManager::UpdatePerView()
{
    if (!camera_ || !perViewData_) {
        return;
    }

    Matrix4x4 cameraMatrix = camera_->GetWorldMatrix();
    perViewData_->cameraPosition = {
        cameraMatrix.m[3][0],
        cameraMatrix.m[3][1],
        cameraMatrix.m[3][2],
    };

    cameraMatrix.m[3][0] = 0.0f;
    cameraMatrix.m[3][1] = 0.0f;
    cameraMatrix.m[3][2] = 0.0f;

    perViewData_->billboardMatrix = cameraMatrix;
    perViewData_->viewProjection = camera_->GetViewProjectionMatrix();
}

void EffectManager::RemoveDeadEffects()
{
    for (size_t i = 0; i < activeEffects_.size();) {
        if (activeEffects_[i].isAlive) {
            ++i;
            continue;
        }

        ReleaseEffectLight(activeEffects_[i]);
        UnmapActiveEffectResource(activeResources_[i]);
        retiredResources_.push_back(std::move(activeResources_[i]));

        activeEffects_.erase(activeEffects_.begin() + static_cast<std::ptrdiff_t>(i));
        activeResources_.erase(activeResources_.begin() + static_cast<std::ptrdiff_t>(i));
    }
}

void EffectManager::UnmapActiveEffectResource(ActiveEffectResource& resource)
{
    if (resource.emitterResource && resource.emitterData) {
        resource.emitterResource->Unmap(0, nullptr);
        resource.emitterData = nullptr;
    }

    if (resource.perFrameResource && resource.perFrameData) {
        resource.perFrameResource->Unmap(0, nullptr);
        resource.perFrameData = nullptr;
    }

    if (resource.effectSettingsResource && resource.effectSettingsData) {
        resource.effectSettingsResource->Unmap(0, nullptr);
        resource.effectSettingsData = nullptr;
    }

    if (resource.renderParameterResource && resource.renderParameterData) {
        resource.renderParameterResource->Unmap(0, nullptr);
        resource.renderParameterData = nullptr;
    }

    if (resource.fieldResource && resource.fieldData) {
        resource.fieldResource->Unmap(0, nullptr);
        resource.fieldData = nullptr;
    }
}

void EffectManager::ReleaseActiveEffectDescriptors(ActiveEffectResource& resource)
{
    ReleaseActiveEffectDescriptor(resource.particleUavIndex);
    ReleaseActiveEffectDescriptor(resource.particleSrvIndex);
    ReleaseActiveEffectDescriptor(resource.freeListIndexUavIndex);
    ReleaseActiveEffectDescriptor(resource.freeListUavIndex);
}

void EffectManager::ReleaseActiveEffectDescriptor(uint32_t& descriptorIndex)
{
    if (descriptorIndex == kInvalidDescriptorIndex) {
        return;
    }

    srvManager_->Free(descriptorIndex);
    descriptorIndex = kInvalidDescriptorIndex;
}

void EffectManager::ReleaseRetiredResources()
{
    for (ActiveEffectResource& resource : retiredResources_) {
        ReleaseActiveEffectDescriptors(resource);
    }

    retiredResources_.clear();
}

void EffectManager::PreDraw()
{
    particleRenderManager_->PreDraw();
}

void EffectManager::Draw()
{
    if (activeEffects_.empty()) {
        return;
    }

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
    ID3D12PipelineState* currentPipelineState = nullptr;

    for (size_t i = 0; i < activeEffects_.size(); ++i) {
        const ActiveEffect& activeEffect = activeEffects_[i];
        if (!activeEffect.isAlive) {
            continue;
        }

        std::unordered_map<std::string, EffectRuntime>::const_iterator runtimeIterator =
            effects_.find(activeEffect.effectName);
        if (runtimeIterator == effects_.end()) {
            continue;
        }

        const EffectRuntime& runtime = runtimeIterator->second;
        const ActiveEffectResource& resource = activeResources_[i];

        ID3D12PipelineState* graphicsPipelineState = runtime.graphicsPipelineState.Get();
        if (currentPipelineState != graphicsPipelineState) {
            commandList->SetPipelineState(graphicsPipelineState);
            currentPipelineState = graphicsPipelineState;
        }

        commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
        commandList->SetGraphicsRootConstantBufferView(3, perViewResource_->GetGPUVirtualAddress());
        commandList->SetGraphicsRootConstantBufferView(4, fogConstantBufferView_);
        commandList->SetGraphicsRootConstantBufferView(5, resource.renderParameterResource->GetGPUVirtualAddress());
        commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        commandList->SetGraphicsRootDescriptorTable(1, resource.particleSrvHandleGPU);
        commandList->SetGraphicsRootDescriptorTable(
            2,
            TextureManager::GetInstance()->GetSrvHandleGPU(runtime.texturePath));

        if (runtime.renderType == ParticleRenderType::Trail) {
            const uint32_t trailVertexCount =
                (runtime.renderParameter.maxTrailPoints - 1) * 6;
            commandList->IASetVertexBuffers(0, 0, nullptr);
            commandList->IASetIndexBuffer(nullptr);
            commandList->DrawInstanced(trailVertexCount, 1, 0, 0);
            continue;
        }

        const D3D12_VERTEX_BUFFER_VIEW& vertexBufferView =
            particleMeshManager_->GetVertexBufferView(runtime.meshType);
        const D3D12_INDEX_BUFFER_VIEW& indexBufferView =
            particleMeshManager_->GetIndexBufferView(runtime.meshType);
        const uint32_t indexCount = particleMeshManager_->GetIndexCount(runtime.meshType);

        commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
        commandList->IASetIndexBuffer(&indexBufferView);

        commandList->DrawIndexedInstanced(indexCount, kMaxGPUParticle, 0, 0, 0);
    }
}

void EffectManager::DrawFieldDebug() const
{
    DebugRenderer* debugRenderer = DebugRenderer::GetInstance();
    const Vector4 fieldColors[] = {
        { 0.1f, 0.9f, 1.0f, 1.0f },
        { 0.2f, 1.0f, 0.3f, 1.0f },
        { 1.0f, 0.25f, 0.15f, 1.0f },
        { 0.85f, 0.2f, 1.0f, 1.0f },
    };

    for (size_t effectIndex = 0; effectIndex < activeEffects_.size(); ++effectIndex) {
        if (!activeEffects_[effectIndex].isAlive ||
            effectIndex >= activeResources_.size()) {
            continue;
        }

        const ActiveEffectResource& resource = activeResources_[effectIndex];
        if (!resource.fieldData) {
            continue;
        }

        uint32_t fieldCount = resource.fieldData->fieldCount;
        if (fieldCount > kMaxParticleFields) {
            fieldCount = kMaxParticleFields;
        }

        for (uint32_t fieldIndex = 0; fieldIndex < fieldCount; ++fieldIndex) {
            const ParticleField& field = resource.fieldData->fields[fieldIndex];
            int32_t colorIndex = field.type;
            if (colorIndex < 0 || colorIndex >= 4) {
                colorIndex = 0;
            }
            const Vector4 color = fieldColors[colorIndex];

            debugRenderer->AddWireSphere(
                field.position,
                field.radius,
                color,
                2.0f,
                32);

            float centerMarkSize = field.radius * 0.08f;
            if (centerMarkSize < 0.15f) {
                centerMarkSize = 0.15f;
            }
            debugRenderer->AddLine(
                { field.position.x - centerMarkSize, field.position.y, field.position.z },
                { field.position.x + centerMarkSize, field.position.y, field.position.z },
                color,
                3.0f);
            debugRenderer->AddLine(
                { field.position.x, field.position.y - centerMarkSize, field.position.z },
                { field.position.x, field.position.y + centerMarkSize, field.position.z },
                color,
                3.0f);
            debugRenderer->AddLine(
                { field.position.x, field.position.y, field.position.z - centerMarkSize },
                { field.position.x, field.position.y, field.position.z + centerMarkSize },
                color,
                3.0f);

            if (field.type != static_cast<int32_t>(ParticleFieldType::Wind) &&
                field.type != static_cast<int32_t>(ParticleFieldType::Vortex)) {
                continue;
            }

            const float directionLength = std::sqrt(
                field.direction.x * field.direction.x +
                field.direction.y * field.direction.y +
                field.direction.z * field.direction.z);
            if (directionLength <= 0.0001f) {
                continue;
            }

            const Vector3 direction = {
                field.direction.x / directionLength,
                field.direction.y / directionLength,
                field.direction.z / directionLength,
            };
            float arrowLength = field.radius * 0.65f;
            if (arrowLength > 3.5f) {
                arrowLength = 3.5f;
            }
            if (arrowLength < 0.5f) {
                arrowLength = 0.5f;
            }

            Vector3 arrowStart = field.position;
            if (field.type == static_cast<int32_t>(ParticleFieldType::Vortex)) {
                arrowStart.x -= direction.x * arrowLength;
                arrowStart.y -= direction.y * arrowLength;
                arrowStart.z -= direction.z * arrowLength;
            }
            const Vector3 arrowEnd = {
                field.position.x + direction.x * arrowLength,
                field.position.y + direction.y * arrowLength,
                field.position.z + direction.z * arrowLength,
            };
            debugRenderer->AddLine(arrowStart, arrowEnd, color, 4.0f);

            Vector3 helperAxis = { 0.0f, 1.0f, 0.0f };
            if (std::abs(direction.y) > 0.9f) {
                helperAxis = { 1.0f, 0.0f, 0.0f };
            }
            Vector3 perpendicular = {
                direction.y * helperAxis.z - direction.z * helperAxis.y,
                direction.z * helperAxis.x - direction.x * helperAxis.z,
                direction.x * helperAxis.y - direction.y * helperAxis.x,
            };
            const float perpendicularLength = std::sqrt(
                perpendicular.x * perpendicular.x +
                perpendicular.y * perpendicular.y +
                perpendicular.z * perpendicular.z);
            if (perpendicularLength <= 0.0001f) {
                continue;
            }
            perpendicular.x /= perpendicularLength;
            perpendicular.y /= perpendicularLength;
            perpendicular.z /= perpendicularLength;

            const float arrowHeadLength = arrowLength * 0.22f;
            const Vector3 arrowHeadBase = {
                arrowEnd.x - direction.x * arrowHeadLength,
                arrowEnd.y - direction.y * arrowHeadLength,
                arrowEnd.z - direction.z * arrowHeadLength,
            };
            const Vector3 arrowHeadLeft = {
                arrowHeadBase.x + perpendicular.x * arrowHeadLength,
                arrowHeadBase.y + perpendicular.y * arrowHeadLength,
                arrowHeadBase.z + perpendicular.z * arrowHeadLength,
            };
            const Vector3 arrowHeadRight = {
                arrowHeadBase.x - perpendicular.x * arrowHeadLength,
                arrowHeadBase.y - perpendicular.y * arrowHeadLength,
                arrowHeadBase.z - perpendicular.z * arrowHeadLength,
            };
            debugRenderer->AddLine(arrowEnd, arrowHeadLeft, color, 4.0f);
            debugRenderer->AddLine(arrowEnd, arrowHeadRight, color, 4.0f);
        }
    }
}

void EffectManager::SetBlendMode(BlendMode blendMode)
{
    currentBlendMode_ = blendMode;
}

void EffectManager::SetFogConstantBufferView(D3D12_GPU_VIRTUAL_ADDRESS fogConstantBufferView)
{
    fogConstantBufferView_ = fogConstantBufferView;
}

void EffectManager::SetCamera(Camera* camera)
{
    camera_ = camera;
}

void EffectManager::TransitionResource(
    ID3D12Resource* resource,
    D3D12_RESOURCE_STATES& currentState,
    D3D12_RESOURCE_STATES nextState)
{
    if (currentState == nextState) {
        return;
    }

    D3D12_RESOURCE_BARRIER barrier {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = resource;
    barrier.Transition.StateBefore = currentState;
    barrier.Transition.StateAfter = nextState;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    dxCommon_->GetCommandList()->ResourceBarrier(1, &barrier);
    currentState = nextState;
}

void EffectManager::InsertUavBarrier(ID3D12Resource* resource)
{
    D3D12_RESOURCE_BARRIER barrier {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.UAV.pResource = resource;

    dxCommon_->GetCommandList()->ResourceBarrier(1, &barrier);
}

void EffectManager::Finalize()
{
    if (!instance_) {
        return;
    }

    if (instance_->dxCommon_) {
        instance_->dxCommon_->WaitForGPU();
    }

    for (ActiveEffect& activeEffect : instance_->activeEffects_) {
        instance_->ReleaseEffectLight(activeEffect);
    }

    for (ActiveEffectResource& resource : instance_->activeResources_) {
        instance_->UnmapActiveEffectResource(resource);
        instance_->ReleaseActiveEffectDescriptors(resource);
    }

    for (ActiveEffectResource& resource : instance_->retiredResources_) {
        instance_->UnmapActiveEffectResource(resource);
        instance_->ReleaseActiveEffectDescriptors(resource);
    }

    instance_->activeEffects_.clear();
    instance_->activeResources_.clear();
    instance_->retiredResources_.clear();
    instance_->effects_.clear();

    instance_->materialResource_.Reset();
    instance_->perViewResource_.Reset();
    instance_->perViewData_ = nullptr;

    instance_->initializeRootSignature_.Reset();
    instance_->initializePipelineState_.Reset();
    instance_->emitRootSignature_.Reset();
    instance_->updateRootSignature_.Reset();
    instance_->trailInitializePipelineState_.Reset();
    instance_->fieldRootSignature_.Reset();
    instance_->particleFieldPipelineState_.Reset();
    instance_->trailFieldPipelineState_.Reset();

    instance_->particleRenderManager_.reset();
    instance_->particleMeshManager_.reset();

    instance_->dxCommon_ = nullptr;
    instance_->srvManager_ = nullptr;
    instance_->camera_ = nullptr;
    instance_->fogConstantBufferView_ = 0;

    instance_.reset();
}

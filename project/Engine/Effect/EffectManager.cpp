#include "EffectManager.h"

#include "Engine/StringUtility/StringUtility.h"
#include "Engine/TextureManager/TextureManager.h"
#include "Engine/math/MatrixMath.h"
#include "externals/json.hpp"

#include <cassert>
#include <cstddef>
#include <filesystem>
#include <fstream>

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
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    camera_ = camera;

    particleRenderManager_ = std::make_unique<ParticleRenderManager>();
    particleRenderManager_->Initialize(dxCommon_);

    particleMeshManager_ = std::make_unique<ParticleMeshManager>();
    particleMeshManager_->Initialize(dxCommon_);

    CreateMaterialResource();
    CreatePerViewResource();

    CreateInitializeRootSignature();
    CreateInitializePipeline();
    CreateEmitRootSignature();
    CreateUpdateRootSignature();

    RegisterDefaultEffects();
    WarmUpEffects();
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

void EffectManager::WarmUpEffects()
{
    if (effects_.empty()) {
        return;
    }

    const Vector3 warmUpPosition = { 0.0f, -10000.0f, 0.0f };
    const float warmUpDuration = deltaTime_ * 3.0f;

    for (const std::pair<const std::string, EffectRuntime>& effect : effects_) {
        const EffectHandle handle = StartEffect(effect.first, warmUpPosition, false, warmUpDuration, nullptr);
        if (handle == kInvalidEffectHandle) {
            continue;
        }

        const size_t activeEffectIndex = FindActiveEffectIndex(handle);
        if (activeEffectIndex == static_cast<size_t>(-1)) {
            continue;
        }

        UpdateActiveEffect(activeEffectIndex);
    }
}

EffectManager::EffectRuntime EffectManager::CreateEffectRuntime(const EffectData& effectData)
{
    EffectRuntime runtime {};
    runtime.data = effectData;
    ApplyEffectConfig(effectData, runtime);
    runtime.emitPipelineState = CreateComputePipeline(emitRootSignature_.Get(), effectData.emitShaderPath);
    runtime.updatePipelineState = CreateComputePipeline(updateRootSignature_.Get(), effectData.updateShaderPath);

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

    const nlohmann::json& emitter = config.contains("Emitter") ? config.at("Emitter") : config;
    const nlohmann::json& particle = config.contains("Particle") ? config.at("Particle") : config;
    const nlohmann::json& simulation = config.contains("Simulation") ? config.at("Simulation") : config;
    const nlohmann::json& render = config.contains("Render") ? config.at("Render") : config;

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

    runtime.settings.enableGravity = simulation.value("EnableGravity", false) ? 1 : 0;
    runtime.settings.gravity = simulation.value("Gravity", runtime.settings.gravity);
    runtime.settings.enableDrag = simulation.value("EnableDrag", false) ? 1 : 0;
    runtime.settings.drag = simulation.value("Drag", runtime.settings.drag);
    runtime.settings.enableNoise = simulation.value("EnableNoise", false) ? 1 : 0;
    runtime.settings.noiseStrength = simulation.value("NoiseStrength", runtime.settings.noiseStrength);
    runtime.settings.enableAttraction = simulation.value("EnableAttraction", false) ? 1 : 0;
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

    if (render.contains("BlendMode")) {
        runtime.blendMode = ParseBlendMode(render.at("BlendMode").get<std::string>());
    }

    runtime.defaultLoop = render.value("Loop", runtime.defaultLoop);
    runtime.duration = render.value("Duration", runtime.duration);
}

Microsoft::WRL::ComPtr<ID3D12PipelineState> EffectManager::CreateComputePipeline(
    ID3D12RootSignature* rootSignature,
    const std::string& shaderPath)
{
    Microsoft::WRL::ComPtr<IDxcBlob> computeShaderBlob = dxCommon_->CompileShader(StringUtility::ConvertString(shaderPath),L"cs_6_0");

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
    auto runtimeIterator = effects_.find(effectName);
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
    DispatchInitialize(resource);

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

bool EffectManager::StopEffect(EffectHandle handle)
{
    const size_t index = FindActiveEffectIndex(handle);
    if (index == static_cast<size_t>(-1)) {
        return false;
    }

    activeEffects_[index].isAlive = false;
    return true;
}

bool EffectManager::IsEffectAlive(EffectHandle handle) const
{
    return FindActiveEffectIndex(handle) != static_cast<size_t>(-1);
}

EffectManager::ActiveEffectResource EffectManager::CreateActiveEffectResource(
    const EffectRuntime& runtime,
    const Vector3& position)
{
    ActiveEffectResource resource {};

    resource.particleResource = CreateUavBufferResource(sizeof(ParticleCS) * kMaxGPUParticle,L"EffectManager::ParticleBuffer");
    resource.freeListIndexResource = CreateUavBufferResource(sizeof(int32_t),L"EffectManager::FreeListIndex");
    resource.freeListResource = CreateUavBufferResource(sizeof(uint32_t) * kMaxGPUParticle,L"EffectManager::FreeList");

    resource.particleUavHandleGPU = CreateStructuredBufferUAV(
        resource.particleResource.Get(),
        kMaxGPUParticle,
        sizeof(ParticleCS),
        resource.particleUavIndex);
    resource.particleSrvHandleGPU = CreateStructuredBufferSRV(
        resource.particleResource.Get(),
        kMaxGPUParticle,
        sizeof(ParticleCS),
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
        initializeRootSignature_.Get(),
        "resources/Shaders/Effects/Common/InitializeParticle.CS.hlsl");
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

void EffectManager::DispatchInitialize(ActiveEffectResource& resource)
{
    TransitionResource(resource.particleResource.Get(), resource.particleState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    TransitionResource(resource.freeListIndexResource.Get(), resource.freeListIndexState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    TransitionResource(resource.freeListResource.Get(), resource.freeListState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
    ID3D12DescriptorHeap* descriptorHeaps[] = { srvManager_->GetDescriptorHeap() };
    commandList->SetDescriptorHeaps(1, descriptorHeaps);

    commandList->SetComputeRootSignature(initializeRootSignature_.Get());
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
    commandList->SetComputeRootDescriptorTable(1, resource.freeListIndexUavHandleGPU);
    commandList->SetComputeRootDescriptorTable(2, resource.freeListUavHandleGPU);
    commandList->SetComputeRootConstantBufferView(3, resource.perFrameResource->GetGPUVirtualAddress());
    commandList->SetComputeRootConstantBufferView(4, resource.effectSettingsResource->GetGPUVirtualAddress());
    commandList->SetComputeRootConstantBufferView(5, resource.emitterResource->GetGPUVirtualAddress());
    commandList->Dispatch((kMaxGPUParticle + 255) / 256, 1, 1);
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

    auto runtimeIterator = effects_.find(activeEffect.effectName);
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

    resource.emitterData->translate = activeEffect.position;
    resource.emitterData->prevTranslate = activeEffect.prevPosition;
    resource.emitterData->radius = runtime.emitRadius;
    resource.emitterData->count = runtime.emitCount;
    resource.emitterData->frequency = runtime.emitFrequency;

    if (activeEffect.isLoop) {
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

    if (resource.emitterData->emit != 0) {
        DispatchEmit(runtime, resource);
        InsertUavBarrier(resource.particleResource.Get());
    }

    DispatchUpdate(runtime, resource);
    InsertUavBarrier(resource.particleResource.Get());

    TransitionResource(resource.particleResource.Get(), resource.particleState, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

    if (activeEffect.duration >= 0.0f && activeEffect.duration <= resource.age) {
        activeEffect.isAlive = false;
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
}

void EffectManager::ReleaseActiveEffectDescriptors(ActiveEffectResource& resource)
{
    auto freeDescriptor = [this](uint32_t& descriptorIndex) {
        if (descriptorIndex == kInvalidDescriptorIndex) {
            return;
        }

        srvManager_->Free(descriptorIndex);
        descriptorIndex = kInvalidDescriptorIndex;
    };

    freeDescriptor(resource.particleUavIndex);
    freeDescriptor(resource.particleSrvIndex);
    freeDescriptor(resource.freeListIndexUavIndex);
    freeDescriptor(resource.freeListUavIndex);
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
    particleRenderManager_->PreDraw(currentBlendMode_);
}

void EffectManager::Draw()
{
    if (activeEffects_.empty()) {
        return;
    }

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

    for (size_t i = 0; i < activeEffects_.size(); ++i) {
        const ActiveEffect& activeEffect = activeEffects_[i];
        if (!activeEffect.isAlive) {
            continue;
        }

        const auto runtimeIterator = effects_.find(activeEffect.effectName);
        if (runtimeIterator == effects_.end()) {
            continue;
        }

        const EffectRuntime& runtime = runtimeIterator->second;
        const ActiveEffectResource& resource = activeResources_[i];

        particleRenderManager_->PreDraw(runtime.blendMode);
        commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
        commandList->SetGraphicsRootConstantBufferView(3, perViewResource_->GetGPUVirtualAddress());
        commandList->SetGraphicsRootConstantBufferView(4, fogConstantBufferView_);
        commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        const D3D12_VERTEX_BUFFER_VIEW& vertexBufferView =
            particleMeshManager_->GetVertexBufferView(runtime.meshType);
        const D3D12_INDEX_BUFFER_VIEW& indexBufferView =
            particleMeshManager_->GetIndexBufferView(runtime.meshType);
        const uint32_t indexCount = particleMeshManager_->GetIndexCount(runtime.meshType);

        commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
        commandList->IASetIndexBuffer(&indexBufferView);

        commandList->SetGraphicsRootDescriptorTable(1, resource.particleSrvHandleGPU);
        commandList->SetGraphicsRootDescriptorTable(
            2,
            TextureManager::GetInstance()->GetSrvHandleGPU(runtime.texturePath));

        commandList->DrawIndexedInstanced(indexCount, kMaxGPUParticle, 0, 0, 0);
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

    instance_->particleRenderManager_.reset();
    instance_->particleMeshManager_.reset();

    instance_->dxCommon_ = nullptr;
    instance_->srvManager_ = nullptr;
    instance_->camera_ = nullptr;
    instance_->fogConstantBufferView_ = 0;

    instance_.reset();
}

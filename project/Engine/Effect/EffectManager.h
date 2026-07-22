#pragma once

#include "Engine/Camera/Camera.h"
#include "Engine/DirectXCommon/DirectXCommon.h"
#include "Engine/Particle/MeshManager/ParticleMeshManager.h"
#include "Engine/Particle/ParticleData.h"
#include "Engine/Particle/ParticleRenderManager.h"
#include "Engine/SrvManager/SrvManager.h"
#include "Engine/blend/BlendUtil.h"
#include "Engine/math/EngineStruct.h"
#include "externals/json.hpp"

#include <d3d12.h>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <wrl.h>

using EffectHandle = uint32_t;
inline constexpr EffectHandle kInvalidEffectHandle = 0xffffffffu;
using EffectPositionProvider = std::function<Vector3()>;

struct EffectData {
    std::string effectName;
    std::string effectDirectory;
    std::string jsonPath;
    std::string emitShaderPath;
    std::string updateShaderPath;
    std::string vertexShaderPath;
    std::string pixelShaderPath;
};

struct ActiveEffect {
    EffectHandle handle = kInvalidEffectHandle;
    std::string effectName;
    Vector3 position;
    Vector3 prevPosition;
    EffectPositionProvider positionProvider;
    float duration = -1.0f;
    uint32_t pointLightHandle = 0xffffffffu;
    float lightBaseIntensity = 0.0f;
    float lightFadeStartAge = 0.0f;
    float lightFadeDuration = 0.0f;
    bool isLoop = false;
    bool isEmitting = true;
    bool isLightFading = false;
    bool isAlive = false;
};

class EffectManager {
public:
    static EffectManager* GetInstance();
    static void Finalize();

public:
    class ConstructorKey {
        ConstructorKey() = default;
        friend class EffectManager;
    };

    explicit EffectManager(ConstructorKey);
    ~EffectManager() = default;

    EffectManager(const EffectManager&) = delete;
    EffectManager& operator=(const EffectManager&) = delete;

    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, Camera* camera);

    void BeginWarmUp();
    void UpdateWarmUp();
    bool IsWarmUpComplete() const;
    float GetWarmUpProgress() const;

    void RegisterEffect(const EffectData& effectData);
    EffectHandle PlayEffect(const std::string& effectName, const Vector3& position);
    EffectHandle PlayLoopEffect(const std::string& effectName, const Vector3& position, float duration = -1.0f);
    EffectHandle AttachEffect(
        const std::string& effectName,
        EffectPositionProvider positionProvider,
        float duration = -1.0f);

    template <class Target>
    EffectHandle AttachEffect(const std::string& effectName, Target* target, float duration = -1.0f)
    {
        if (!target) {
            return kInvalidEffectHandle;
        }

        return AttachEffect(
            effectName,
            std::bind(static_cast<const Vector3& (Target::*)() const>(&Target::GetTranslate), target),
            duration);
    }

    template <class Target>
    EffectHandle AttachEffect(const std::string& effectName, const std::unique_ptr<Target>& target, float duration = -1.0f)
    {
        return AttachEffect(effectName, target.get(), duration);
    }

    bool SetEffectPosition(EffectHandle handle, const Vector3& position);
    bool StopEffect(EffectHandle handle);
    // シーンに属する再生中エフェクトだけを停止する。
    // シェーダーやパイプラインなどの共通リソースは保持する。
    void StopAllEffects();
    bool IsEffectAlive(EffectHandle handle) const;

    void Update();
    void PreDraw();
    void Draw();
    void DrawFieldDebug() const;

    void SetBlendMode(BlendMode blendMode);
    void SetFogConstantBufferView(D3D12_GPU_VIRTUAL_ADDRESS fogConstantBufferView);
    void SetCamera(Camera* camera);
    void UpdatePerView();

    const std::vector<ActiveEffect>& GetActiveEffects() const { return activeEffects_; }

private:
    enum class ParticleRenderType {
        Mesh,
        Trail,
    };

    enum class EmitterShape : int32_t {
        Sphere,
        Box,
        Cone,
        Cylinder,
        Circle,
    };

    enum class ParticleFieldType : int32_t {
        Wind,
        Attractor,
        Repulsor,
        Vortex,
    };

    struct Material {
        Vector4 color;
        int32_t enableLighting;
        float padding[3];
        Matrix4x4 uvTransform;
        float alphaReference;
        float padding2[3];
    };

    struct EffectSettings {
        Vector4 startColor = { 1.0f, 1.0f, 1.0f, 1.0f };
        Vector4 endColor = { 1.0f, 1.0f, 1.0f, 0.0f };
        Vector3 velocity = { 0.0f, 0.0f, 0.0f };
        float lifeTime = 1.0f;
        float startScale = 1.0f;
        float endScale = 1.0f;
        float startRotation = 0.0f;
        float rotationSpeed = 0.0f;
        int32_t emitterShape = static_cast<int32_t>(EmitterShape::Sphere);
        int32_t enableGravity = 0;
        int32_t enableDrag = 0;
        int32_t enableNoise = 0;
        int32_t enableAttraction = 0;
        float gravity = -9.8f;
        float drag = 0.95f;
        float noiseStrength = 1.0f;
        float attractionStrength = 1.0f;
        float padding[3] = {};
    };

    struct ParticleRenderParameter {
        float dissolveThreshold = 0.0f;
        float emissionStrength = 1.0f;
        float uvScrollSpeedX = 0.0f;
        float uvScrollSpeedY = 0.0f;
        Vector4 trailStartColor = { 1.0f, 1.0f, 1.0f, 1.0f };
        Vector4 trailEndColor = { 1.0f, 1.0f, 1.0f, 0.0f };
        float trailLifeTime = 0.5f;
        float trailStartWidth = 0.2f;
        float trailEndWidth = 0.0f;
        float trailTextureTiling = 1.0f;
        float trailMinVertexDistance = 0.05f;
        float trailBreakDistance = 5.0f;
        uint32_t maxTrailPoints = 64;
        int32_t faceCamera = 1;
        float trailRootExtension = 0.0f;
        float trailPadding[3] = {};
    };

    struct TrailPoint {
        Vector3 position;
        float age = 0.0f;
        Vector3 velocity = { 0.0f, 0.0f, 0.0f };
        uint32_t isActive = 0;
    };

    struct ParticleField {
        Vector3 position = { 0.0f, 0.0f, 0.0f };
        float radius = 1.0f;
        Vector3 direction = { 0.0f, 1.0f, 0.0f };
        float strength = 1.0f;
        int32_t type = static_cast<int32_t>(ParticleFieldType::Wind);
        float falloff = 1.0f;
        float padding[2] = {};
    };

    struct ParticleFieldDefinition {
        ParticleField field;
        int32_t isLocal = 1;
    };

    static constexpr uint32_t kMaxParticleFields = 16;

    struct ParticleFieldCollection {
        ParticleField fields[kMaxParticleFields];
        uint32_t fieldCount = 0;
        float padding[3] = {};
    };

    struct EffectLightSettings {
        Vector4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
        Vector3 offset = { 0.0f, 0.0f, 0.0f };
        float intensity = 1.0f;
        float radius = 5.0f;
        float decay = 2.0f;
        float fadeDuration = 0.25f;
        int32_t enabled = 0;
        int32_t followEmitter = 1;
        int32_t fadeOut = 1;
    };

    struct EffectRuntime {
        EffectData data;
        Microsoft::WRL::ComPtr<ID3D12PipelineState> emitPipelineState;
        Microsoft::WRL::ComPtr<ID3D12PipelineState> updatePipelineState;
        Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState;
        std::string texturePath = "resources/Particles/circle.png";
        std::string vertexShaderPath = "resources/Shaders/Effects/Common/Particle.VS.hlsl";
        std::string pixelShaderPath = "resources/Shaders/Effects/Common/Particle.PS.hlsl";
        ParticleRenderType renderType = ParticleRenderType::Mesh;
        ParticleMeshManager::ParticleMeshType meshType = ParticleMeshManager::ParticleMeshType::Board;
        BlendMode blendMode = kBlendModeAdd;
        bool depthTest = true;
        bool depthWrite = false;
        D3D12_CULL_MODE cullMode = D3D12_CULL_MODE_NONE;
        uint32_t emitCount = 128;
        float emitRadius = 0.2f;
        float emitFrequency = 0.05f;
        float duration = 1.5f;
        bool defaultLoop = false;
        EffectSettings settings;
        ParticleRenderParameter renderParameter;
        EffectLightSettings lightSettings;
        ParticleFieldDefinition fields[kMaxParticleFields];
        uint32_t fieldCount = 0;
    };

    static constexpr uint32_t kInvalidDescriptorIndex = 0xffffffffu;

    struct ActiveEffectResource {
        ParticleRenderType renderType = ParticleRenderType::Mesh;
        Microsoft::WRL::ComPtr<ID3D12Resource> particleResource;
        Microsoft::WRL::ComPtr<ID3D12Resource> freeListIndexResource;
        Microsoft::WRL::ComPtr<ID3D12Resource> freeListResource;
        Microsoft::WRL::ComPtr<ID3D12Resource> emitterResource;
        Microsoft::WRL::ComPtr<ID3D12Resource> perFrameResource;
        Microsoft::WRL::ComPtr<ID3D12Resource> effectSettingsResource;
        Microsoft::WRL::ComPtr<ID3D12Resource> renderParameterResource;
        Microsoft::WRL::ComPtr<ID3D12Resource> fieldResource;

        EmitterSphere* emitterData = nullptr;
        PerFrame* perFrameData = nullptr;
        EffectSettings* effectSettingsData = nullptr;
        ParticleRenderParameter* renderParameterData = nullptr;
        ParticleFieldCollection* fieldData = nullptr;

        uint32_t particleUavIndex = kInvalidDescriptorIndex;
        uint32_t particleSrvIndex = kInvalidDescriptorIndex;
        uint32_t freeListIndexUavIndex = kInvalidDescriptorIndex;
        uint32_t freeListUavIndex = kInvalidDescriptorIndex;

        D3D12_GPU_DESCRIPTOR_HANDLE particleUavHandleGPU {};
        D3D12_GPU_DESCRIPTOR_HANDLE particleSrvHandleGPU {};
        D3D12_GPU_DESCRIPTOR_HANDLE freeListIndexUavHandleGPU {};
        D3D12_GPU_DESCRIPTOR_HANDLE freeListUavHandleGPU {};

        D3D12_RESOURCE_STATES particleState = D3D12_RESOURCE_STATE_COMMON;
        D3D12_RESOURCE_STATES freeListIndexState = D3D12_RESOURCE_STATE_COMMON;
        D3D12_RESOURCE_STATES freeListState = D3D12_RESOURCE_STATE_COMMON;

        float age = 0.0f;
        bool hasEmitted = false;
    };

private:
    void RegisterDefaultEffects();
    bool WarmUpEffect(const std::string& effectName);
    EffectRuntime CreateEffectRuntime(const EffectData& effectData);
    void ApplyEffectConfig(const EffectData& effectData, EffectRuntime& runtime);
    void ApplyRenderParameterConfig(const nlohmann::json& config, ParticleRenderParameter& renderParameter);
    Microsoft::WRL::ComPtr<ID3D12PipelineState> CreateComputePipeline(
        const std::string& effectName,
        const std::string& shaderStage,
        ID3D12RootSignature* rootSignature,
        const std::string& shaderPath);

    EffectHandle StartEffect(
        const std::string& effectName,
        const Vector3& position,
        bool isLoop,
        float duration,
        EffectPositionProvider positionProvider);
    ActiveEffectResource CreateActiveEffectResource(const EffectRuntime& runtime, const Vector3& position);
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateUavBufferResource(size_t sizeInBytes, const wchar_t* name);
    D3D12_GPU_DESCRIPTOR_HANDLE CreateStructuredBufferUAV(
        ID3D12Resource* resource,
        uint32_t elementCount,
        uint32_t stride,
        uint32_t& descriptorIndex);
    D3D12_GPU_DESCRIPTOR_HANDLE CreateStructuredBufferSRV(
        ID3D12Resource* resource,
        uint32_t elementCount,
        uint32_t stride,
        uint32_t& descriptorIndex);

    void CreateMaterialResource();
    void CreatePerViewResource();

    void CreateInitializeRootSignature();
    void CreateInitializePipeline();
    void CreateEmitRootSignature();
    void CreateUpdateRootSignature();
    void CreateFieldRootSignature();
    void CreateFieldPipelines();

    void DispatchInitialize(ActiveEffectResource& resource);
    void DispatchEmit(const EffectRuntime& runtime, ActiveEffectResource& resource);
    void DispatchUpdate(const EffectRuntime& runtime, ActiveEffectResource& resource);
    void DispatchFields(const EffectRuntime& runtime, ActiveEffectResource& resource);

    EffectHandle AllocateEffectHandle();
    size_t FindActiveEffectIndex(EffectHandle handle) const;
    void UpdateActiveEffect(size_t index);
    void CreateEffectLight(const EffectRuntime& runtime, ActiveEffect& activeEffect);
    void UpdateEffectLight(
        const EffectRuntime& runtime,
        ActiveEffect& activeEffect,
        const ActiveEffectResource& resource);
    void BeginEffectFadeOut(
        const EffectRuntime& runtime,
        ActiveEffect& activeEffect,
        const ActiveEffectResource& resource);
    void ReleaseEffectLight(ActiveEffect& activeEffect);
    void UpdateEffectFields(
        const EffectRuntime& runtime,
        const ActiveEffect& activeEffect,
        ActiveEffectResource& resource);
    void RemoveDeadEffects();
    void UnmapActiveEffectResource(ActiveEffectResource& resource);
    void ReleaseActiveEffectDescriptors(ActiveEffectResource& resource);
    void ReleaseActiveEffectDescriptor(uint32_t& descriptorIndex);
    void ReleaseRetiredResources();

    void TransitionResource(
        ID3D12Resource* resource,
        D3D12_RESOURCE_STATES& currentState,
        D3D12_RESOURCE_STATES nextState);
    void InsertUavBarrier(ID3D12Resource* resource);

private:
    static std::unique_ptr<EffectManager> instance_;

    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    Camera* camera_ = nullptr;

    BlendMode currentBlendMode_ = kBlendModeAdd;
    float deltaTime_ = 1.0f / 60.0f;
    D3D12_GPU_VIRTUAL_ADDRESS fogConstantBufferView_ = 0;

    std::unique_ptr<ParticleRenderManager> particleRenderManager_;
    std::unique_ptr<ParticleMeshManager> particleMeshManager_;

    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
    Material materialData_ {};

    Microsoft::WRL::ComPtr<ID3D12Resource> perViewResource_;
    PerView* perViewData_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> initializeRootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> initializePipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> trailInitializePipelineState_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> emitRootSignature_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> updateRootSignature_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> fieldRootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> particleFieldPipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> trailFieldPipelineState_;

    std::unordered_map<std::string, EffectRuntime> effects_;
    std::vector<ActiveEffect> activeEffects_;
    std::vector<ActiveEffectResource> activeResources_;
    std::vector<ActiveEffectResource> retiredResources_;
    EffectHandle nextEffectHandle_ = 1;

    std::vector<std::string> warmUpEffectNames_;
    size_t warmUpEffectIndex_ = 0;
    bool isWarmUpComplete_ = false;

    static constexpr uint32_t kMaxGPUParticle = 1024;
    static constexpr uint32_t kMaxTrailPoints = 64;
};

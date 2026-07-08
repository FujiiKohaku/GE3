#pragma once

#include "Engine/Camera/Camera.h"
#include "Engine/DirectXCommon/DirectXCommon.h"
#include "Engine/Particle/MeshManager/ParticleMeshManager.h"
#include "Engine/Particle/ParticleData.h"
#include "Engine/Particle/ParticleRenderManager.h"
#include "Engine/SrvManager/SrvManager.h"
#include "Engine/blend/BlendUtil.h"
#include "Engine/math/EngineStruct.h"

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
};

struct ActiveEffect {
    EffectHandle handle = kInvalidEffectHandle;
    std::string effectName;
    Vector3 position;
    Vector3 prevPosition;
    EffectPositionProvider positionProvider;
    float duration = -1.0f;
    bool isLoop = false;
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
            [target]() {
                return target->GetTranslate();
            },
            duration);
    }

    template <class Target>
    EffectHandle AttachEffect(const std::string& effectName, const std::unique_ptr<Target>& target, float duration = -1.0f)
    {
        return AttachEffect(effectName, target.get(), duration);
    }

    bool SetEffectPosition(EffectHandle handle, const Vector3& position);
    bool StopEffect(EffectHandle handle);
    bool IsEffectAlive(EffectHandle handle) const;

    void Update();
    void PreDraw();
    void Draw();

    void SetBlendMode(BlendMode blendMode);
    void SetFogConstantBufferView(D3D12_GPU_VIRTUAL_ADDRESS fogConstantBufferView);
    void SetCamera(Camera* camera);
    void UpdatePerView();

    const std::vector<ActiveEffect>& GetActiveEffects() const { return activeEffects_; }

private:
    enum class EmitterShape : int32_t {
        Sphere,
        Box,
        Cone,
        Cylinder,
        Circle,
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

    struct EffectRuntime {
        EffectData data;
        Microsoft::WRL::ComPtr<ID3D12PipelineState> emitPipelineState;
        Microsoft::WRL::ComPtr<ID3D12PipelineState> updatePipelineState;
        std::string texturePath = "resources/Particles/circle.png";
        ParticleMeshManager::ParticleMeshType meshType = ParticleMeshManager::ParticleMeshType::Board;
        BlendMode blendMode = kBlendModeAdd;
        uint32_t emitCount = 128;
        float emitRadius = 0.2f;
        float emitFrequency = 0.05f;
        float duration = 1.5f;
        bool defaultLoop = false;
        EffectSettings settings;
    };

    static constexpr uint32_t kInvalidDescriptorIndex = 0xffffffffu;

    struct ActiveEffectResource {
        Microsoft::WRL::ComPtr<ID3D12Resource> particleResource;
        Microsoft::WRL::ComPtr<ID3D12Resource> freeListIndexResource;
        Microsoft::WRL::ComPtr<ID3D12Resource> freeListResource;
        Microsoft::WRL::ComPtr<ID3D12Resource> emitterResource;
        Microsoft::WRL::ComPtr<ID3D12Resource> perFrameResource;
        Microsoft::WRL::ComPtr<ID3D12Resource> effectSettingsResource;

        EmitterSphere* emitterData = nullptr;
        PerFrame* perFrameData = nullptr;
        EffectSettings* effectSettingsData = nullptr;

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
    void WarmUpEffects();
    EffectRuntime CreateEffectRuntime(const EffectData& effectData);
    void ApplyEffectConfig(const EffectData& effectData, EffectRuntime& runtime);
    Microsoft::WRL::ComPtr<ID3D12PipelineState> CreateComputePipeline(
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

    void DispatchInitialize(ActiveEffectResource& resource);
    void DispatchEmit(const EffectRuntime& runtime, ActiveEffectResource& resource);
    void DispatchUpdate(const EffectRuntime& runtime, ActiveEffectResource& resource);

    EffectHandle AllocateEffectHandle();
    size_t FindActiveEffectIndex(EffectHandle handle) const;
    void UpdateActiveEffect(size_t index);
    void RemoveDeadEffects();
    void UnmapActiveEffectResource(ActiveEffectResource& resource);
    void ReleaseActiveEffectDescriptors(ActiveEffectResource& resource);
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
    Microsoft::WRL::ComPtr<ID3D12RootSignature> emitRootSignature_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> updateRootSignature_;

    std::unordered_map<std::string, EffectRuntime> effects_;
    std::vector<ActiveEffect> activeEffects_;
    std::vector<ActiveEffectResource> activeResources_;
    std::vector<ActiveEffectResource> retiredResources_;
    EffectHandle nextEffectHandle_ = 1;

    static constexpr uint32_t kMaxGPUParticle = 1024;
};

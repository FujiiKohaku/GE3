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
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <wrl.h>

struct EffectData {
    std::string effectName;
    std::string emitShaderPath;
    std::string updateShaderPath;
    bool isLoop = false;
};

struct ActiveEffect {
    std::string effectName;
    Vector3 position;
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
    void PlayEffect(const std::string& effectName, const Vector3& position);

    void Update();
    void PreDraw();
    void Draw();

    void SetBlendMode(BlendMode blendMode);

    const std::vector<ActiveEffect>& GetActiveEffects() const { return activeEffects_; }

private:
    struct Material {
        Vector4 color;
        int32_t enableLighting;
        float padding[3];
        Matrix4x4 uvTransform;
        float alphaReference;
        float padding2[3];
    };

    struct EffectRuntime {
        EffectData data;
        Microsoft::WRL::ComPtr<ID3D12PipelineState> emitPipelineState;
        Microsoft::WRL::ComPtr<ID3D12PipelineState> updatePipelineState;
        std::string texturePath = "resources/Particles/circle.png";
        ParticleMeshManager::ParticleMeshType meshType = ParticleMeshManager::ParticleMeshType::Board;
        uint32_t emitCount = 128;
        float emitRadius = 0.2f;
        float emitFrequency = 0.05f;
        float duration = 1.5f;
    };

    static constexpr uint32_t kInvalidDescriptorIndex = 0xffffffffu;

    struct ActiveEffectResource {
        Microsoft::WRL::ComPtr<ID3D12Resource> particleResource;
        Microsoft::WRL::ComPtr<ID3D12Resource> freeListIndexResource;
        Microsoft::WRL::ComPtr<ID3D12Resource> freeListResource;
        Microsoft::WRL::ComPtr<ID3D12Resource> emitterResource;
        Microsoft::WRL::ComPtr<ID3D12Resource> perFrameResource;

        EmitterSphere* emitterData = nullptr;
        PerFrame* perFrameData = nullptr;

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
    EffectRuntime CreateEffectRuntime(const EffectData& effectData);
    Microsoft::WRL::ComPtr<ID3D12PipelineState> CreateComputePipeline(
        ID3D12RootSignature* rootSignature,
        const std::string& shaderPath);

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
    void UpdatePerView();

    void CreateInitializeRootSignature();
    void CreateInitializePipeline();
    void CreateEmitRootSignature();
    void CreateUpdateRootSignature();

    void DispatchInitialize(ActiveEffectResource& resource);
    void DispatchEmit(const EffectRuntime& runtime, ActiveEffectResource& resource);
    void DispatchUpdate(const EffectRuntime& runtime, ActiveEffectResource& resource);

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

    static constexpr uint32_t kMaxGPUParticle = 1024;
};

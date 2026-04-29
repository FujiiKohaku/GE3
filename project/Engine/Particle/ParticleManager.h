#pragma once

#include "Engine/Camera/Camera.h"
#include "Engine/DirectXCommon/DirectXCommon.h"
#include "Engine/SrvManager/SrvManager.h"
#include "Engine/TextureManager/TextureManager.h"
#include "Engine/blend/BlendUtil.h"

#include "MeshManager/ParticleMeshManager.h"
#include "ParticleData.h"
#include "ParticleEmitter.h"
#include "ParticleRenderManager.h"

#include <d3d12.h>
#include <list>
#include <memory>
#include <string>
#include <unordered_map>
#include <wrl.h>

#include"../math/EngineStruct.h"
class ParticleManager {
public:
    static ParticleManager* GetInstance();
    static void Finalize();

public:
    class ConstructorKey {
        ConstructorKey() = default;
        friend class ParticleManager;
    };

    explicit ParticleManager(ConstructorKey);
    ~ParticleManager() = default;

    ParticleManager(const ParticleManager&) = delete;
    ParticleManager& operator=(const ParticleManager&) = delete;

public:
    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, Camera* camera);
    void Update();
    void PreDraw();
    void Draw();

    void SetBlendMode(BlendMode mode);

public:
    void CreateParticleGroup(
        const std::string& name,
        const std::string& textureFilePath,
        ParticleMeshManager::ParticleMeshType meshType);

    void Emit(const std::string& name, const Vector3& position, uint32_t count);
    void EmitFire(const std::string& name, const Vector3& position, uint32_t count);
    void EmitRing(const std::string& name, const Vector3& position, uint32_t count);
    void AddParticle(const std::string& name, const Particle& particle);

private:
    struct Material {
        Vector4 color;
        int32_t enableLighting;
        float padding[3];
        Matrix4x4 uvTransform;
        float alphaReference;
        float padding2[3];
    };

    struct ParticleGroup {
        std::string texturePath;
        std::list<Particle> particles;

        Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource;
        ParticleForGPU* instanceData = nullptr;
        D3D12_GPU_DESCRIPTOR_HANDLE instancingSrvHandleGPU {};

        uint32_t numInstance = 0;

        ParticleMeshManager::ParticleMeshType meshType = ParticleMeshManager::ParticleMeshType::Board;
    };

private:
    void CreateMaterialResource();

private:
    void InitializeCPUParticle();

private:
    void InitializeGPUParticle();
    void CreateGPUParticleResource();
    void CreateGPUParticleUAV();
    void CreateGPUParticleSRV();
    void CreateGPUParticleInitializeRootSignature();
    void CreateGPUParticleInitializePipeline();
    void DispatchInitializeGPUParticle();
    void CreatePerViewResource();

private:
    static std::unique_ptr<ParticleManager> instance_;

private:
    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    Camera* camera_ = nullptr;

private:
    BlendMode currentBlendMode_ = kBlendModeAdd;

    static const uint32_t kNumMaxInstance = 500;
    float deltaTime_ = 1.0f / 60.0f;
    bool useBillboard_ = true;
    bool useGPUParticle_ = true;

private:
    std::unordered_map<std::string, ParticleGroup> particleGroups_;

private:
    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
    Material materialData_ {};

private:
    std::unique_ptr<ParticleRenderManager> particleRenderManager_;
    std::unique_ptr<ParticleMeshManager> particleMeshManager_;
    std::unique_ptr<ParticleEmitter> particleEmitter_;

private:
    static const uint32_t kMaxGPUParticle = 1024;

    Microsoft::WRL::ComPtr<ID3D12Resource> gpuParticleResource_;

    uint32_t gpuParticleUavIndex_ = 0;
    uint32_t gpuParticleSrvIndex_ = 0;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> gpuParticleInitializeRootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> gpuParticleInitializePipelineState_;
    D3D12_GPU_DESCRIPTOR_HANDLE gpuParticleUavHandleGPU_ {};
    D3D12_GPU_DESCRIPTOR_HANDLE gpuParticleSrvHandleGPU_ {};
    Microsoft::WRL::ComPtr<ID3D12Resource> perViewResource_;
    PerView* perViewData_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> emitterResource_;
    EmitterSphere* emitterData_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> emitParticleRootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> emitParticlePipelineState_;
    void CreateEmitterResource();
    void UpdateEmitter();
    void CreateEmitParticleRootSignature();
    void CreateEmitParticlePipeline();
    void DispatchEmitParticle();
    Microsoft::WRL::ComPtr<ID3D12Resource> perFrameResource_;
    PerFrame* perFrameData_ = nullptr;
    float time_ = 0.0f;

    void CreatePerFrameResource();
    void UpdatePerFrame();
};
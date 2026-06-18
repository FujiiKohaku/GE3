#pragma once

#include "Engine/Camera/Camera.h"
#include "Engine/DirectXCommon/DirectXCommon.h"
#include "Engine/Particle/MeshManager/ParticleMeshManager.h"
#include "Engine/Particle/ParticleData.h"
#include "Engine/Particle/ParticleRenderManager.h"
#include "Engine/SrvManager/SrvManager.h"
#include "Engine/blend/BlendUtil.h"
#include "Engine/math/GPUParticleStruct.h"

#include <d3d12.h>
#include <list>
#include <memory>
#include <string>
#include <unordered_map>
#include <wrl.h>

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
    static constexpr uint32_t kInvalidDescriptorIndex = 0xffffffffu;
    static constexpr uint32_t kNumMaxInstance = 500;

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
        uint32_t instancingSrvIndex = kInvalidDescriptorIndex;

        uint32_t numInstance = 0;

        ParticleMeshManager::ParticleMeshType meshType = ParticleMeshManager::ParticleMeshType::Board;
    };

private:
    void CreateMaterialResource();
    void CreatePerViewResource();
    void UpdatePerView();

private:
    static std::unique_ptr<ParticleManager> instance_;

private:
    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    Camera* camera_ = nullptr;

    BlendMode currentBlendMode_ = kBlendModeAdd;
    float deltaTime_ = 1.0f / 60.0f;
    bool useBillboard_ = true;

    std::unordered_map<std::string, ParticleGroup> particleGroups_;

    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
    Material materialData_ {};

    Microsoft::WRL::ComPtr<ID3D12Resource> perViewResource_;
    PerView* perViewData_ = nullptr;

    std::unique_ptr<ParticleRenderManager> particleRenderManager_;
    std::unique_ptr<ParticleMeshManager> particleMeshManager_;
};

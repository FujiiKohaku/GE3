#pragma once
#include "Engine/blend/BlendUtil.h"
#include "Engine/Camera/Camera.h"
#include "Engine/DirectXCommon/DirectXCommon.h"
#include "ParticleMeshManager.h"
#include "ParticleRenderManager.h"
#include "ParticleEmitter.h"
#include "Engine/SrvManager/SrvManager.h"
#include "Engine/TextureManager/TextureManager.h"
#include <d3d12.h>
#include <list>
#include <memory>
#include <random>
#include <string>
#include <unordered_map>
#include <wrl.h>
#include"ParticleData.h"
class ParticleManager {
public:
    static ParticleManager* GetInstance();
    static void Finalize();

    struct Material {
        Vector4 color;
        int32_t enableLighting;
        float padding[3];
        Matrix4x4 uvTransform;
    };



    struct ParticleGroup {
        std::string texturePath;
        std::list<Particle> particles;
        Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource;
        ParticleForGPU* instanceData = nullptr;
        D3D12_GPU_DESCRIPTOR_HANDLE instancingSrvHandleGPU {};
        uint32_t numInstance = 0;
    };

public:
    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, Camera* camera);
    void Update();
    void PreDraw();
    void Draw();

    void SetBlendMode(BlendMode mode) { currentBlendMode_ = mode; }

    void CreateParticleGroup(const std::string& name, const std::string& textureFilePath);
    void Emit(const std::string& name, const Vector3& position, uint32_t count);
    void EmitFire(const std::string& name, const Vector3& position, uint32_t count);

  

private:
    static std::unique_ptr<ParticleManager> instance_;

    ParticleManager(const ParticleManager&) = delete;
    ParticleManager& operator=(const ParticleManager&) = delete;

public:
    class ConstructorKey {
        ConstructorKey() = default;
        friend class ParticleManager;
    };

    explicit ParticleManager(ConstructorKey);
    ~ParticleManager() = default;

private:
    void CreateMaterialResource();

private:
    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    Camera* camera_ = nullptr;

    BlendMode currentBlendMode_ = kBlendModeAdd;
    static const uint32_t kNumMaxInstance = 500;

    std::unordered_map<std::string, ParticleGroup> particleGroups_;

    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
    Material materialData_ {};

    bool useBillboard_ = true;
    float deltaTime = 1.0f / 60.0f;

    std::unique_ptr<ParticleRenderManager> particleRenderManager_;
    std::unique_ptr<ParticleMeshManager> particleMeshManager_;
    std::unique_ptr<ParticleEmitter> particleEmitter_;
};
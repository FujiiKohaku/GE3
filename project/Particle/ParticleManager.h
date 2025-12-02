#pragma once

#include "Camera.h"
#include "DirectXCommon.h"
#include "SrvManager.h"
#include "TextureManager.h"
#include "blendutil.h"

#include <d3d12.h>
#include <list>
#include <random>
#include <string>
#include <wrl.h>

class ParticleManager {
public:
    // =========================================================
    // Singleton Access
    // =========================================================
    static ParticleManager* GetInstance()
    {
        static ParticleManager instance;
        return &instance;
    }

    // =========================================================
    // GPUに送る構造体
    // =========================================================
    struct Material {
        Vector4 color;
        int32_t enableLighting;
        float padding[3];
        Matrix4x4 uvTransform;
    };

    struct TransformationMatrix {
        Matrix4x4 WVP;
        Matrix4x4 World;
    };

    struct DirectionalLight {
        Vector4 color;
        Vector3 direction;
        float intensity;
    };

    struct VertexData {
        Vector4 position;
        Vector2 texcoord;
        Vector3 normal;
    };

    struct Particle {
        Transform transform;
        Vector3 velocity;
        Vector4 color;
        float lifeTime;
        float currentTime;
    };

    struct ParticleForGPU {
        Matrix4x4 WVP;
        Matrix4x4 World;
        Vector4 color;
    };

    struct Shockwave {
        Vector3 pos;
        float lifeTime;
        float currentTime;
        float startScale;
        float endScale;
    };

    struct Emitter {
        Transform transform;
        uint32_t count;
        float frequency;
        float frequencyTime;
    };

    enum class ParticleType {
        Normal,
        Fire,
        Smoke,
        Spark,
        FireWork
    };
    ParticleType type = ParticleType::Normal;

public:
    // =========================================================
    // 基本操作
    // =========================================================
    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, Camera* camera);
    void Update();
    void PreDraw();
    void Draw();

    // BlendMode の setter
    void SetBlendMode(BlendMode mode) { currentBlendMode_ = mode; }

    // UI
    void ImGui();

    // Emit系
    std::list<Particle> Emit(const Emitter& emitter, std::mt19937& randomEngine);
    std::list<Particle> EmitFire(const Emitter& emitter, std::mt19937& randomEngine);
    std::list<Particle> EmitSmoke(const Emitter& emitter, std::mt19937& randomEngine);
    std::list<Particle> EmitLightning(const Emitter& emitter, std::mt19937& randomEngine);
    std::list<Particle> EmitFireworkSpark(const Emitter& emitter, std::mt19937& randomEngine);

private:
    // =========================================================
    // Singleton Safety
    // =========================================================
    ParticleManager() = default;
    ~ParticleManager() = default;

    ParticleManager(const ParticleManager&) = delete;
    ParticleManager& operator=(const ParticleManager&) = delete;

private:
    // =========================================================
    // 内部処理
    // =========================================================
    void CreateRootSignature();
    void CreateGraphicsPipeline();
    void CreateInstancingBuffer();
    void CreateSrvBuffer();
    void CreateBoardMesh();
    void UpdateTransforms();

    Particle MakeNewParticle(std::mt19937& randomEngine, const Vector3& translate);
    Particle MakeNewParticleFire(std::mt19937& randomEngine, const Vector3& translate);
    Particle MakeNewParticleSmoke(std::mt19937& randomEngine, const Vector3& translate);
    Particle MakeNewParticleLightning(std::mt19937& randomEngine, const Vector3& translate);
    Particle MakeFireworkSpark(std::mt19937& randomEngine, const Vector3& center);

private:
    // =========================================================
    // DirectX / 外部依存
    // =========================================================
    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    Camera* camera_ = nullptr;

    // =========================================================
    // RootSignature / PSO
    // =========================================================
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineStates[kCountOfBlendMode];
    int currentBlendMode_ = kBlendModeNormal;

    // =========================================================
    // GPU リソース
    // =========================================================
    Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource;
    ParticleForGPU* instanceData_ = nullptr;
    uint32_t numInstance_ = 0;

    static const uint32_t kNumMaxInstance = 100;

    std::list<Particle> particles;
    std::list<Shockwave> shokParticles;

    D3D12_GPU_DESCRIPTOR_HANDLE instancingSrvHandleGPU_ {};
    D3D12_GPU_DESCRIPTOR_HANDLE srvHandle {};

    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource;
    Microsoft::WRL::ComPtr<ID3D12Resource> transformResource;
    Microsoft::WRL::ComPtr<ID3D12Resource> lightResource;

    Material materialData_ {};
    TransformationMatrix transformData_ {};
    DirectionalLight lightData_ {};

    VertexData vertices[4];
    uint32_t indexList[6] = { 0, 1, 2, 0, 2, 3 };

    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexResource;

    D3D12_VERTEX_BUFFER_VIEW vertexBufferView {};
    D3D12_INDEX_BUFFER_VIEW indexBufferView {};

    Transform transformBoard_ = {
        { 1.0f, 1.0f, 1.0f },
        { 0.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 0.0f }
    };

    std::random_device seedGenerator_;
    std::mt19937 randomEngine_;
    bool useBillboard_ = true;

    float kdeltaTime = 0.1f;
    Emitter emitter {};
};

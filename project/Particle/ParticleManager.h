#pragma once
#include "Camera.h"
#include "DirectXCommon.h"
#include "SrvManager.h"
#include "TextureManager.h"
#include "blendutil.h" // BlendMode ここで定義

#include <d3d12.h>
#include <list>
#include <random>
#include <string>
#include <wrl.h>

class ParticleManager {
public:
    using ComPtr = Microsoft::WRL::ComPtr<ID3D12Resource>;

    // =========================================================
    // GPU に送る構造体
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

    struct Emitter {
        Transform transform; // エミッタのトランスフォーム
        uint32_t count; // 発生数
        float frequency; // 発生頻度
        float frequencyTime; // 頻度用時刻
    };

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
    void ImGui();
    // 発せ関数
    std::list<Particle> Emit(const Emitter& emitter, std::mt19937& randomEngine);

private:
    // =========================================================
    // 内部処理
    // =========================================================
    void CreateRootSignature();
    void CreateGraphicsPipeline();
    void CreateInstancingBuffer();
    void CreateSrvBuffer();
    void CreateBoardMesh();
    void InitTransforms();
    void UpdateTransforms();
    Particle MakeNewParticle(std::mt19937& randomEngine, const Vector3& translate);
    Particle MakeNewParticleFire(std::mt19937& randomEngine);

private:
    // =========================================================
    // DirectX / 外部依存
    // =========================================================
    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    Camera* camera_ = nullptr;

    // =========================================================
    // RootSignature / PSO（BlendMode対応）
    // =========================================================
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineStates[kCountOfBlendMode];
    int currentBlendMode_ = kBlendModeNormal;

    // =========================================================
    // GPU リソース（SRV / StructuredBuffer / CB）
    // =========================================================
    ComPtr instancingResource;
    ParticleForGPU* instanceData_ = nullptr; // map しっぱなし用

    uint32_t numInstance_ = 0;
    static const uint32_t kNumMaxInstance = 100;

    // パーティクル本体
    std::list<Particle> particles;

    // SRV の GPU ハンドル
    D3D12_GPU_DESCRIPTOR_HANDLE instancingSrvHandleGPU_ {};
    D3D12_GPU_DESCRIPTOR_HANDLE srvHandle {};

    // Material / Transform / Light 用 CB
    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource;
    Microsoft::WRL::ComPtr<ID3D12Resource> transformResource;
    Microsoft::WRL::ComPtr<ID3D12Resource> lightResource;

    Material materialData_ {};
    TransformationMatrix transformData_ {};
    DirectionalLight lightData_ {};

    // 板ポリ mesh
    VertexData vertices[4];
    uint32_t indexList[6] = { 0, 1, 2, 0, 2, 3 };

    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexResource;

    D3D12_VERTEX_BUFFER_VIEW vertexBufferView {};
    D3D12_INDEX_BUFFER_VIEW indexBufferView {};
    // パーティクル板ポリ用の Transform
    Transform transformBoard_ = {
        { 1.0f, 1.0f, 1.0f }, // scale
        { 0.0f, 0.0f, 0.0f }, // rotate
        { 0.0f, 0.0f, 0.0f } // translate
    };

    // 乱数
    std::random_device seedGenerator_;
    std::mt19937 randomEngine_;
    bool useBillboard_ = true;
    float kdeltaTime;
    Emitter emitter {};
    
};

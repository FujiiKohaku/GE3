#pragma once
#include "DirectXCommon.h"
#include "MatrixMath.h"
#include "SrvManager.h"
#include "TextureManager.h"
#include "camera.h"
#include <d3d12.h>
#include <random>
#include <string>
#include <wrl.h>

class ParticleManager {
public:
    using ComPtr = Microsoft::WRL::ComPtr<ID3D12Resource>;

    // -------- Material --------
    struct Material {
        Vector4 color;
        int32_t enableLighting;
        float padding[3];
        Matrix4x4 uvTransform;
    };

    // -------- Transform Matrix --------
    struct TransformationMatrix {
        Matrix4x4 WVP;
        Matrix4x4 World;
    };

    // -------- Light --------
    struct DirectionalLight {
        Vector4 color;
        Vector3 direction;
        float intensity;
    };

    // -------- Vertex --------
    struct VertexData {
        Vector4 position;
        Vector2 texcoord;
        Vector3 normal;
    };

    // -------- Transform --------
    struct Transform {
        Vector3 scale;
        Vector3 rotate;
        Vector3 translate;
    };

    // -------- Particle --------
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

public:
    // -------- Public Methods --------
    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, Camera* camera);
    void Update();
    void Draw();
    void PreDraw();

private:
    // -------- RootSig & PSO --------
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;

    void CreateRootSignature();
    void CreateGraphicsPipeline();
    void CreateInstancingBuffer();
    void CreateSrvBuffer();
    void InitTransforms();
    void UpdateTransforms();
    void CreateBoardMesh();

    Particle MakeNewParticle(std::mt19937& randomEngine);

private:
    // -------- External ----------
    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    Camera* camera_ = nullptr;

    // -------- RNG --------
    std::random_device seedGenerator_;
    std::mt19937 randomEngine_;

    // -------- Instance Count --------
    static const uint32_t kNumMaxInstance = 10;
    Particle particles[kNumMaxInstance] = {};
    uint32_t numInstance_ = 0; // 描画すべきインスタンス数
    // -------- GPU Buffers --------
    ComPtr vertexResource; // VB
    ComPtr indexResource; // IB
    ComPtr materialResource; // CB: Material
    ComPtr transformResource; // CB: Board Transform
    ComPtr lightResource; // CB: Light
    ComPtr instancingResource; // StructuredBuffer
    ParticleForGPU* instanceData_ = nullptr;
    // -------- Views --------
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView {};
    D3D12_INDEX_BUFFER_VIEW indexBufferView {};

    // -------- SRV Handles --------
    D3D12_GPU_DESCRIPTOR_HANDLE instancingSrvHandleGPU_ {};
    D3D12_GPU_DESCRIPTOR_HANDLE srvHandle {};

    // -------- Mesh --------
    VertexData vertices[4];
    uint32_t indexList[6] = { 0, 1, 2, 0, 2, 3 };

    // -------- Board Transform --------
    Transform transformBoard_ = {
        { 1.0f, 1.0f, 1.0f },
        { 0.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, -30.0f }
    };

    // -------- CPU Cache --------
    Material materialData_;
    TransformationMatrix transformData_;
    DirectionalLight lightData_;
    
};

#pragma once
#include "DirectXCommon.h"
#include "MatrixMath.h"
#include "SrvManager.h"
#include "TextureManager.h"
#include <d3d12.h>
#include <string>
#include <wrl.h>

class ParticleManager {
public:
    using ComPtr = Microsoft::WRL::ComPtr<ID3D12Resource>;

    struct VertexData {
        Vector4 position;
        Vector2 texcoord;
        Vector3 normal;
    };

    struct Transform {
        Vector3 scale = { 1, 1, 1 };
        Vector3 rotate = { 0, 0, 0 };
        Vector3 translate = { 0, 0, 0 };
    };
    struct TransformationMatrix {
        Matrix4x4 WVP; // ワールド × ビュー × プロジェクション
        Matrix4x4 World; // ワールド行列（法線用）
    };

    Transform transform_;

    // ============================================================
    //           初期化・更新・描画
    // ============================================================
    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);
    void Update();
    void Draw(ID3D12GraphicsCommandList* commandList);

private:
    // ============================================================
    //           RootSignature & PSO（Sprite / Object3D と同じ設計）
    // ============================================================
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;

    void CreateRootSignature();
    void CreateGraphicsPipeline();
    void CreateInstancingBuffer();

    void CreateSrvBuffer();

private:
    // ============================================================
    //           GPU リソース（Vertex / Index / WVP / Material）
    // ============================================================
    ComPtr vertexResource_;
    ComPtr indexResource_;
    ComPtr materialResource_;
    ComPtr wvpResource_;
    ComPtr instancingResource;
    // GPU View
    D3D12_VERTEX_BUFFER_VIEW vbv_ {};
    D3D12_INDEX_BUFFER_VIEW ibv_ {};

    // CPU 書き込みポインタ
    Matrix4x4* wvpData_ = nullptr;
    Vector4* materialData_ = nullptr;

    // SRV (SrvManager で管理するテクスチャ)
    uint32_t textureSrvIndex_ = 0;

    // 外部依存
    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    std::string texturePath_;
    // シリアライズ用Blob
    ID3DBlob* signatureBlob = nullptr;
    ID3DBlob* errorBlob = nullptr;
    // パーティクルの最大個数

    // 2. const を使う
    const uint32_t kNumInstance = 100; // OK (コンパイラによる)
  
};

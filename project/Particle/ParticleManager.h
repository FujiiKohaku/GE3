#pragma once
#include "DirectXCommon.h"
#include "MatrixMath.h"
#include "SrvManager.h"
#include "TextureManager.h"
#include "camera.h"
#include <d3d12.h>
#include <string>
#include <wrl.h>
class ParticleManager {
public:
    using ComPtr = Microsoft::WRL::ComPtr<ID3D12Resource>;

    // マテリアル情報（色・ライティング・UV変換など）
    struct Material {
        Vector4 color; // 色
        int32_t enableLighting; // ライティング有効フラグ
        float padding[3]; // アライメント調整
        Matrix4x4 uvTransform; // UV変換行列
    };

    // 変換行列（WVP＋World）
    struct TransformationMatrix {
        Matrix4x4 WVP;
        Matrix4x4 World;
    };

    // 平行光源データ
    struct DirectionalLight {
        Vector4 color;
        Vector3 direction;
        float intensity;
    };

    // マテリアル外部データ（ファイルパス・テクスチャ番号）
    struct MaterialData {
        std::string textureFilePath;
        uint32_t textureIndex = 0;
    };
    Material materialData_ {};
    TransformationMatrix transformData_ {};
    DirectionalLight lightData_ {};
    // 頂点データ
    struct VertexData {
        Vector4 position;
        Vector2 texcoord;
        Vector3 normal;
    };

    // モデル全体データ（頂点配列＋マテリアル）
    struct ModelData {
        std::vector<VertexData> vertices;
        MaterialData material;
    };

    // Transform情報（スケール・回転・位置）
    struct Transform {
        Vector3 scale;
        Vector3 rotate;
        Vector3 translate;
    };

    Transform transform_;

    // ============================================================
    //           初期化・更新・描画
    // ============================================================
    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, Camera* camera);
    void Update();
    void Draw();
    void PreDraw();

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
    void InitTransforms();
    void UpdateTransforms();
    void CreateBoardMesh();

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
    Transform transforms[100] = {};

    Camera* camera_;
    // instancing 用 GPUハンドル
    D3D12_GPU_DESCRIPTOR_HANDLE instancingSrvHandleGPU_ = {};

    // ------------------------------
    // パーティクル
    // ▼ 板ポリ用：定数バッファ（GPU の箱）
    // ------------------------------

    // ------------------------------
    // 三角形描画用
    // ------------------------------
    // 三角形描画用
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView {};
    D3D12_HEAP_PROPERTIES uploadHeapProperties {};
    D3D12_RESOURCE_DESC vertexResourceDesc {};

    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource;

    // マテリアル / 変換 / ライト用 CB
    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource;
    Microsoft::WRL::ComPtr<ID3D12Resource> transformResource;
    Microsoft::WRL::ComPtr<ID3D12Resource> lightResource;

    D3D12_GPU_DESCRIPTOR_HANDLE srvHandle {};
    uint32_t indexList[6] = { 0, 1, 2, 0, 2, 3 };
    Microsoft::WRL::ComPtr<ID3D12Resource> indexResource; // IB本体
    D3D12_INDEX_BUFFER_VIEW indexBufferView {}; // IBビュー

    Transform transformBoard_ = {
        { 1.0f, 1.0f, 1.0f }, // scale
        { 0.0f, 0.0f, 0.0f }, // rotate
        { 0.0f, 0.0f, -30.0f }, // translate
    };

   VertexData vertices[4];
};

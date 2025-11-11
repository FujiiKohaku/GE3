#pragma once
#include "DirectXCommon.h"
#include "MatrixMath.h"
#include "SrvManager.h"
#include <d3d12.h>
#include <random>
#include <wrl.h>
/// <summary>
/// パーティクルを管理するマネージャ（シングルトン）
/// </summary>
class ParticleManager {
public:
    // ===== シングルトン =====
    static ParticleManager* GetInstance();

    // ===== 基本処理 =====
    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);
    void Update();
    void Draw();
    void Finalize();
    void CreateParticleGroup(const std::string name, const std::string textureFilePath);

    void Emit(const std::string name, const Vector3& position, uint32_t count);

private:
    // ===== 頂点データ =====
    struct VertexData {
        Vector3 position; // 頂点の座標
        Vector2 texCoord; // テクスチャ座標
    };
    struct Particle {
        Vector3 position;
        Vector3 velocity;
        float lifeTime;
        float currentTime;
    };
    struct InstancingData {
        Matrix4x4 matWorld;
        Vector4 color;
    };

    struct ParticleGroup {
        // ========================
        // マテリアル関連
        // ========================
        std::string textureFilePath; // 使用するテクスチャのファイルパス
        uint32_t textureSrvIndex = 0; // テクスチャ用SRVインデックス

        // ========================
        // パーティクル本体リスト
        // ========================
        std::list<Particle> particles; // パーティクルのリスト（型は後で定義予定）

        // ========================
        // インスタンシング描画用
        // ========================
        uint32_t instancingSrvIndex = 0; // インスタンシングデータ用SRVインデックス
        Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource; // GPUリソース
        uint32_t instanceCount = 0; // インスタンス数
        InstancingData* instancingData = nullptr; // 書き込み用ポインタ
    };
    // ===== シングルトン用 =====
    ParticleManager() = default;
    ~ParticleManager() = default;
    ParticleManager(const ParticleManager&) = delete;
    ParticleManager& operator=(const ParticleManager&) = delete;

    // ===== メンバ =====
    DirectXCommon* dxCommon_ = nullptr; // DirectX共通処理
    SrvManager* srvManager_ = nullptr; // SRVマネージャ
    std::unordered_map<std::string, ParticleGroup> particleGroups_;
    // 乱数生成エンジン
    std::mt19937 randomEngine_;

    // パイプラインステートオブジェクト（PSO）
    Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState = nullptr;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature = nullptr;

    // 頂点バッファ関連
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_; // 頂点バッファ
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_ {};
    Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_; // インデックスバッファ
    D3D12_INDEX_BUFFER_VIEW indexBufferView_ {};
    // CPU側アクセス用ポインタ
    VertexData* vertexData_ = nullptr;
    uint32_t* indexData_ = nullptr;

    static const uint32_t kVertexCount = 4;
    VertexData vertices_[kVertexCount];
    // シリアライズ・エラー出力用
    ID3DBlob* signatureBlob = nullptr;
    ID3DBlob* errorBlob = nullptr;
    static ParticleManager* instance; 


private:
    // ===== 内部処理 =====
    void CreateGraphicsPipeline();
    void CreateRootSignature();
    void CreateVertexBuffer();
};

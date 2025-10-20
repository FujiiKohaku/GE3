#pragma once
#include "Struct.h" // Vector2, Vector3, Vector4, Matrix4x4 など
#include "TextureManager.h" // テクスチャ管理
#include <cstdint> // uint32_t など
#include <d3d12.h> // D3D12関連型（ID3D12Resourceなど）
#include <string> // std::string
#include <wrl.h> // ComPtrスマートポインタ

// 前方宣言
class SpriteManager;

// ===============================================
// Spriteクラス（2D画像描画クラス）
// ===============================================
class Sprite {
public:
    // ===============================
    // 初期化（必要情報を渡して準備）
    // ===============================
    void Initialize(SpriteManager* spriteManager, std::string textureFilePath);

    // 毎フレームの更新処理（行列など）
    void Update();

    // 描画処理（GPUにコマンド送信）
    void Draw();

    // ===============================
    // 各種Getter / Setter
    // ===============================

    // 位置
    const Vector2& GetPosition() const { return position; }
    void SetPosition(const Vector2& pos) { position = pos; }

    // 回転
    float GetRotation() const { return rotation; }
    void SetRotation(float rot) { rotation = rot; }

    // 色
    const Vector4& GetColor() const { return materialData->color; }
    void SetColor(const Vector4& color) { materialData->color = color; }

    // サイズ
    const Vector2& GetSize() const { return size; }
    void SetSize(const Vector2& s) { size = s; }

private:
    // ===============================
    // GPU転送用データ構造
    // ===============================

    // 頂点データ
    struct VertexData {
        Vector4 position; // 位置
        Vector2 texcoord; // UV座標
        Vector3 normal; // 法線（使わないが整合性のため）
    };

    // マテリアルデータ（色情報など）
    struct Material {
        Vector4 color; // RGBAカラー
        int32_t enableLighting; // ライティング有効フラグ（Spriteではfalse）
        float padding[3]; // アラインメント調整
        Matrix4x4 uvTransform; // UV変換行列
    };

    // 変換行列データ（GPU定数バッファ用）
    struct TransformationMatrix {
        Matrix4x4 WVP; // ワールド×ビュー×プロジェクション行列
        Matrix4x4 World; // ワールド行列
    };

    // トランスフォーム情報（位置・回転・拡縮）
    struct Transform {
        Vector3 scale;
        Vector3 rotate;
        Vector3 translate;
    };

    // ===============================
    // メンバ変数
    // ===============================

    // 座標・回転・サイズ
    Vector2 position = { 0.0f, 0.0f };
    float rotation = 0.0f;
    Vector2 size = { 640.0f, 360.0f };

    // トランスフォーム初期値
    Transform transform {
        { 1.0f, 1.0f, 1.0f }, // 拡縮
        { 0.0f, 0.0f, 0.0f }, // 回転
        { 0.0f, 0.0f, 0.0f } // 平行移動
    };

    // SpriteManagerへの参照
    SpriteManager* spriteManager_ = nullptr;

    // ===============================
    // GPUバッファ関連
    // ===============================

    // GPUリソース（バッファ）
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource; // 頂点バッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> indexResource; // インデックスバッファ

    // CPU側アクセス用ポインタ
    VertexData* vertexData = nullptr;
    uint32_t* indexData = nullptr;

    // バッファビュー（使い方情報）
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView {};
    D3D12_INDEX_BUFFER_VIEW indexBufferView {};

    // 変換行列定数バッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResource;
    TransformationMatrix* transformationMatrixData = nullptr;

    // マテリアル定数バッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource;
    Material* materialData = nullptr;

    // ===============================
    // テクスチャ関連
    // ===============================
    D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU_ {}; // GPUハンドル
    int textureIndex = 0; // テクスチャ番号

    // ===============================
    // 内部処理関数
    // ===============================
    void CreateVertexBuffer(); // 頂点バッファ生成
    void CreateMaterialBuffer(); // マテリアルバッファ生成
    void CreateTransformationMatrixBuffer(); // 変換行列バッファ生成
};

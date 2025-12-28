#pragma once
#include "../../math/SpriteStruct.h"
#include"../../System/TextureManager/TextureManager.h" // テクスチャ管理
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
    Sprite()
    {
        static int a = 0;
        a++;
    }

    ~Sprite()
    {
        static int b = 0;
        b++;

        ULONG refCountTemp = vertexResource->Release();
        refCount = refCountTemp;
    }
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
    ULONG refCount = 0;

    // CPU側アクセス用ポインタ
    SpriteVertexData* vertexData = nullptr;
    uint32_t* indexData = nullptr;

    // バッファビュー（使い方情報）
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView {};
    D3D12_INDEX_BUFFER_VIEW indexBufferView {};

    // 変換行列定数バッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResource;
    SpriteTransform* transformationMatrixData = nullptr;

    // マテリアル定数バッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource;
    SpriteMaterial* materialData = nullptr;

    // ===============================
    // テクスチャ関連
    // ===============================
    D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU_ {}; // GPUハンドル
    std::string textureFilePath_;

    // ===============================
    // 内部処理関数
    // ===============================
    void CreateVertexBuffer(); // 頂点バッファ生成
    void CreateMaterialBuffer(); // マテリアルバッファ生成
    void CreateTransformationMatrixBuffer(); // 変換行列バッファ生成

    // ===============================
    // スプライト拡張
    // ===============================

    // アンカーポイント
    Vector2 anchorPoint_ = { 0.0f, 0.0f };

    // 左右フリップ
    bool isFlipX_ = false;
    // 上下フリップ
    bool isFlipY_ = false;

    // テクスチャ左上座標
    Vector2 textureLeftTop_;
    // テクスチャ切り出しサイズ
    Vector2 textureSize_;
    // テクスチャサイズをイメージに合わせる
    void AdjustTextureSize();

public:
    // getter
    const Vector2& GetAnchorPoint() const { return anchorPoint_; }
    // setter
    void SetAnchorPoint(const Vector2& anchorpoint) { anchorPoint_ = anchorpoint; }

    // ========================================
    // Setter
    // ========================================
    void SetIsFlipX(bool isFlipX) { isFlipX_ = isFlipX; }
    void SetIsFlipY(bool isFlipY) { isFlipY_ = isFlipY; }
    // --- セッター ---
    void SetTextureLeftTop(const Vector2& leftTop) { textureLeftTop_ = leftTop; }
    void SetTextureSize(const Vector2& size) { textureSize_ = size; }
    // ========================================
    // Getter
    // ========================================
    bool GetIsFlipX() const { return isFlipX_; }
    bool GetIsFlipY() const { return isFlipY_; }
    // --- ゲッター ---
    const Vector2& GetTextureLeftTop() const { return textureLeftTop_; }
    const Vector2& GetTextureSize() const { return textureSize_; }
};

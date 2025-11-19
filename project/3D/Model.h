#pragma once
#include "DirectXCommon.h"
#include "ModelCommon.h"
#include "Object3d.h"
#include <string>
#include <vector>
#include <wrl.h>

// ===============================================
// モデルクラス：3Dモデルの描画を担当
// ===============================================
class Model {
public:
    // ===============================
    // メイン関数
    // ===============================
    void Initialize(ModelCommon* modelCommon, const std::string& directorypath, const std::string& filename); // 初期化（共通設定の受け取り）
    void Draw(); // 描画

    // ===============================
    // 構造体定義
    // ===============================

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

    // ===============================
    // メンバ変数
    // ===============================
    // Objファイルから読み込んだモデルデータ
    Object3d::ModelData modelData_;

    // getter
    const Object3d::ModelData& GetModelData() const { return modelData_; }
    void SetTexture(const std::string& filePath)
    {
        textureFilePath_ = filePath;
    }

    const std::string& GetTexture() const
    {
        return textureFilePath_;
    }

private:
    std::string textureFilePath_;
    // ===============================
    // GPUリソース関連
    // ===============================

    // 共通設定へのポインタ（DirectXデバイス・コマンドなどを使う）
    ModelCommon* modelCommon_ = nullptr;

    // 頂点バッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_ {}; // 頂点バッファビュー
    Object3d::VertexData* vertexData_ = nullptr; // 頂点データ書き込み用

    // マテリアル用定数バッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
    Object3d::Material* materialData_ = nullptr; // 書き込み用ポインタ
};

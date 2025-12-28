#pragma once
#include "../../Core/DirectXCommon/DirectXCommon.h"
#include "ModelCommon.h"
#include "Object3d.h"

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

    // ===============================
    // メンバ変数
    // ===============================
    // Objファイルから読み込んだモデルデータ
    ModelData modelData_;

    // getter
    const ModelData& GetModelData() const { return modelData_; }
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
    VertexData* vertexData_ = nullptr; // 頂点データ書き込み用

    // マテリアル用定数バッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
    Material* materialData_ = nullptr; // 書き込み用ポインタ
};

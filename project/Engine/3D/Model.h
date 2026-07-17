#pragma once
#include "Engine/DirectXCommon/DirectXCommon.h"
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
    void Initialize(ModelCommon* modelCommon, const ModelData& modelData);
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
    void SetTexture(const std::string& filePath, uint32_t materialIndex = 0);
    const std::string& GetTexture(uint32_t materialIndex = 0) const;
    const MaterialData& GetMaterial(uint32_t materialIndex) const;

private:
    void CreateMeshResources();

    // 共通設定へのポインタ（DirectXデバイス・コマンドなどを使う）
    ModelCommon* modelCommon_ = nullptr;
};

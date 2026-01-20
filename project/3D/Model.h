#pragma once
#include "../math/Object3DStruct.h"
#include "DirectXCommon.h"
#include "ModelCommon.h"
#include "Object3d.h"
#include <d3d12.h>
#include <span>
#include <vector>
#include <wrl.h>
#include"../3D/SkinCluster.h"
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

    // indexバッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> indexResource;
    D3D12_VERTEX_BUFFER_VIEW indexBufferViewSprite;

    // マテリアル用定数バッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
    Material* materialData_ = nullptr; // 書き込み用ポインタ



public:
    SkinCluster CreateSkinCluster(const Microsoft::WRL::ComPtr<ID3D12Device>& device, const Skeleton& skeleton, const ModelData& modelData, const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap, uint32_t descriptorSize);
};

#include "Model.h"
#include "ModelCommon.h"
#include <cassert>
#include <cstring>

// ===============================================
// モデル初期化処理
// ===============================================
void Model::Initialize(ModelCommon* modelCommon, const std::string& directorypath, const std::string& filename)
{
    // ===============================
    // 共通設定を受け取る
    // ===============================
    modelCommon_ = modelCommon; // ModelCommonのポインタを保存

    // ===============================
    // モデルデータ読み込み
    // ===============================
    modelData_ = Object3d::LoadObjFile(directorypath, filename);

    // ===============================
    // 頂点バッファの生成
    // ===============================
    vertexResource_ = modelCommon_->GetDxCommon()->CreateBufferResource(
        sizeof(VertexData) * modelData_.vertices.size());

    // バッファビュー設定
    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = UINT(sizeof(VertexData) * modelData_.vertices.size());
    vertexBufferView_.StrideInBytes = sizeof(VertexData);

    // CPU側から頂点データを転送
    VertexData* vertexData = nullptr;
    vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
    std::memcpy(vertexData, modelData_.vertices.data(),
        sizeof(VertexData) * modelData_.vertices.size());

    // ===============================
    // マテリアルの初期化
    // ===============================
    //materialResource_ = modelCommon_->GetDxCommon()->CreateBufferResource(sizeof(Material));
    //materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));

    //// デフォルト設定（白・ライティング無効）
    //materialData_->color = { 1, 1, 1, 1 };
    //materialData_->enableLighting = true;
    //materialData_->uvTransform = MatrixMath::MakeIdentity4x4();

    // ===============================
    // テクスチャの読み込み・登録
    // ===============================
    TextureManager::GetInstance()->LoadTexture(modelData_.material.textureFilePath);
    modelData_.material.textureIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath(modelData_.material.textureFilePath);
}

// ===============================================
// モデル描画処理
// ===============================================
void Model::Draw()
{
    ID3D12GraphicsCommandList* commandList = modelCommon_->GetDxCommon()->GetCommandList();

    // ===============================
    // 頂点バッファ設定
    // ===============================
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // ===============================
    // マテリアル設定
    // ===============================
    //commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());

    // ===============================
    // テクスチャ設定
    // ===============================
    D3D12_GPU_DESCRIPTOR_HANDLE textureHandle = TextureManager::GetInstance()->GetSrvHandleGPU(modelData_.material.textureFilePath);

    commandList->SetGraphicsRootDescriptorTable(2, textureHandle);

    // ===============================
    // 描画コマンド
    // ===============================
    commandList->DrawInstanced(UINT(modelData_.vertices.size()), 1, 0, 0);
}

#include "SkinningObject3d.h"
#include "MatrixMath.h"
#include "Model.h"
#include "ModelManager.h"
#include "SkinningObject3dManager.h"
#include <cassert>
#include <fstream>
#include <sstream>
#include"SkinCluster.h"
#include<cassert>
#pragma region 初期化処理
void SkinningObject3d::Initialize(SkinningObject3dManager* skinningObject3DManager)
{


    // Manager を保持
    skinningObject3dManager_ = skinningObject3DManager;

    // デフォルトカメラ取得
    camera_ = skinningObject3dManager_->GetDefaultCamera();

    // ================================
    // Transform 定数バッファ
    // ================================
    transformationMatrixResource = skinningObject3dManager_->GetDxCommon()->CreateBufferResource(sizeof(TransformationMatrix));

    transformationMatrixResource->SetName(L"SkinningObject3d::TransformCB");

    transformationMatrixResource->Map(
        0, nullptr, reinterpret_cast<void**>(&transformationMatrixData));

    transformationMatrixData->WVP = MatrixMath::MakeIdentity4x4();
    transformationMatrixData->World = MatrixMath::MakeIdentity4x4();
    transformationMatrixData->WorldInverseTranspose = MatrixMath::MakeIdentity4x4();

    // ================================
    // Material 定数バッファ
    // ================================
    materialResource = skinningObject3dManager_->GetDxCommon()->CreateBufferResource(sizeof(Material));

    materialResource->SetName(L"SkinningObject3d::MaterialCB");

    materialResource->Map(
        0, nullptr, reinterpret_cast<void**>(&materialData_));

    materialData_->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    materialData_->enableLighting = true;
    materialData_->uvTransform = MatrixMath::MakeIdentity4x4();
    materialData_->shininess = 32.0f;

    // ================================
    // Transform 初期値
    // ================================
    transform = {
        { 1.0f, 1.0f, 1.0f },
        { 0.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 0.0f }
    };

    cameraTransform = {
        { 1.0f, 1.0f, 1.0f },
        { 0.3f, 0.0f, 0.0f },
        { 0.0f, 4.0f, -10.0f }
     };

    skinClusterData_ = SkinCluster::CreateSkinCluster(DirectXCommon::GetInstance()->GetDevice(), *playAnimation_->GetSkeleton(), model_->GetModelData(), SrvManager::GetInstance());

    assert(skinningObject3dManager_);
    assert(skinningObject3dManager_->GetDxCommon());
    assert(camera_);

    assert(model_ && "SkinningObject3d::Initialize: model_ is null. Call SetModel() before Initialize().");
    assert(playAnimation_ && "SkinningObject3d::Initialize: playAnimation_ is null. Call SetAnimation() before Initialize().");

  
}

#pragma endregion

#pragma region 更新処理

void SkinningObject3d::Update()
{

    // ================================
    // 各種行列を作成
    // ================================

    worldMatrix_ = MatrixMath::MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);

    Matrix4x4 worldViewProjectionMatrix;

    if (camera_) {
        const Matrix4x4& viewProjectionMatrix = camera_->GetViewProjectionMatrix();
        worldViewProjectionMatrix = MatrixMath::Multiply(worldMatrix_, viewProjectionMatrix);
    } else {
        worldViewProjectionMatrix = worldMatrix_;
    }

    // ================================
    // WVP行列を計算して転送
    // ================================
    transformationMatrixData->WVP = worldViewProjectionMatrix;

    // ワールド行列も送る（ライティングなどで使用）
    transformationMatrixData->World = worldMatrix_;

    Matrix4x4 inv = MatrixMath::Inverse(worldViewProjectionMatrix);
    transformationMatrixData->WorldInverseTranspose = MatrixMath::Transpose(inv);
    if (playAnimation_) {
        SkinCluster::UpdateSkinCluster(skinClusterData_,*playAnimation_->GetSkeleton());
    }
}

#pragma endregion

#pragma region 描画処理
void SkinningObject3d::Draw()
{
    ID3D12GraphicsCommandList* commandList = skinningObject3dManager_->GetDxCommon()->GetCommandList();

    commandList->SetGraphicsRootConstantBufferView(
        0, materialResource->GetGPUVirtualAddress());

    commandList->SetGraphicsRootConstantBufferView(
        1, transformationMatrixResource->GetGPUVirtualAddress());

    //  Skinning 専用（最後）
    commandList->SetGraphicsRootDescriptorTable(
        7, skinClusterData_.paletteSrvHandle.second);

    D3D12_VERTEX_BUFFER_VIEW vbvs[2] = {
        model_->GetVertexBufferView(),
        skinClusterData_.influenceBufferView
    };
    commandList->IASetVertexBuffers(0, 2, vbvs);

    commandList->SetGraphicsRootConstantBufferView(
        4, camera_->GetGPUAddress());

    if (model_) {
        model_->Draw();
    }
}


#pragma endregion




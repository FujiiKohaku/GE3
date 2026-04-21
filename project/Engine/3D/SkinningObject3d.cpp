#include "SkinningObject3d.h"
#include "Engine/math/MatrixMath.h"
#include "Model.h"
#include "ModelManager.h"
#include "SkinCluster.h"
#include "SkinningObject3dManager.h"
#include <cassert>
#include <fstream>
#include <sstream>
#pragma region 初期化処理
void SkinningObject3d::Initialize(SkinningObject3dManager* skinningObject3DManager)
{
    uint32_t vertexCount = 0;
    for (const auto& primitive : model_->GetModelData().primitives) {
        vertexCount += static_cast<uint32_t>(primitive.vertices.size());
    }
    assert(model_ && "Call SetModel() before Initialize()");
    assert(playAnimation_ && "Call SetAnimation() before Initialize()");
    // Manager を保持
    skinningObject3dManager_ = skinningObject3DManager;

    // デフォルトカメラ取得
    camera_ = skinningObject3dManager_->GetDefaultCamera();

    // ================================
    // Transform 定数バッファ
    // ================================
    transformationMatrixResource = skinningObject3dManager_->GetDxCommon()->CreateBufferResource(sizeof(TransformationMatrix));

    transformationMatrixResource->SetName(L"SkinningObject3d::TransformCB");

    transformationMatrixResource->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixData));

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
    materialData_->enableEnvironmentMap = true;
    // =====================================================
    // SkinningInformation 用CB
    // =====================================================
    skinningInformationResource_ = skinningObject3dManager_->GetDxCommon()->CreateBufferResource(sizeof(SkinningInformation));
    assert(skinningInformationResource_);

    skinningInformationResource_->SetName(L"SkinningObject3d::SkinningInformationCB");

    HRESULT hrMap = skinningInformationResource_->Map(0, nullptr, reinterpret_cast<void**>(&skinningInformationData_));
    assert(SUCCEEDED(hrMap));
    assert(skinningInformationData_);

    skinningInformationData_->numVertices = vertexCount;
    // ================================
    // Transform 初期値
    // ================================
    baseTransform_ = {
        { 1.0f, 1.0f, 1.0f },
        { 0.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 0.0f }
    };

    animTransform_ = {
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
    // すきんぐりんぐのリソースを作成
    CreateSkinningResources();

    // assert(model_->GetVertexResource() != nullptr);　

   // TextureManager::GetInstance()->LoadTexture("resources/skyBox.dds");
    //environmentTextureHandle_ = TextureManager::GetInstance()->GetSrvHandleGPU("resources/skyBox.dds");
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
    if (transformationMatrixData == nullptr) {
        return;
    }
    if (camera_ == nullptr) {
        return;
    }

    // ================================
    // 各種行列を作成
    // ================================

    Matrix4x4 baseMatrix = MatrixMath::MakeAffineMatrix(
        baseTransform_.scale,
        baseTransform_.rotate,
        baseTransform_.translate);

    Matrix4x4 animMatrix = MatrixMath::MakeAffineMatrix(
        animTransform_.scale,
        animTransform_.rotate,
        animTransform_.translate);

    worldMatrix_ = MatrixMath::Multiply(animMatrix, baseMatrix);

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
    // ワールド行列も送る（ライティングなどで使用）
    transformationMatrixData->World = worldMatrix_;

    //  ここが重要：World の逆転置
    Matrix4x4 invWorld = MatrixMath::Inverse(worldMatrix_);
    transformationMatrixData->WorldInverseTranspose = MatrixMath::Transpose(invWorld);

    if (playAnimation_) {
        SkinCluster::UpdateSkinCluster(skinClusterData_, *playAnimation_->GetSkeleton());
    }
    // スキニング
    DispatchSkinning();
}

#pragma endregion

#pragma region 描画処理
void SkinningObject3d::Draw()
{
    ID3D12GraphicsCommandList* commandList = skinningObject3dManager_->GetDxCommon()->GetCommandList();

    commandList->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(1, transformationMatrixResource->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(4, camera_->GetGPUAddress());
    commandList->SetGraphicsRootDescriptorTable(7, SrvManager::GetInstance()->GetGPUDescriptorHandle(paletteSrvIndex_));

    D3D12_GPU_DESCRIPTOR_HANDLE textureHandle = TextureManager::GetInstance()->GetSrvHandleGPU(model_->GetModelData().material.textureFilePath);

    commandList->SetGraphicsRootDescriptorTable(2, textureHandle);
    commandList->SetGraphicsRootDescriptorTable(8, SkinningObject3dManager::GetInstance()->GetEnvironmentTexture());

    uint32_t vertexOffset = 0;

    const ModelData& modelData = model_->GetModelData();

    for (const auto& primitive : modelData.primitives) {

        D3D12_VERTEX_BUFFER_VIEW vbView = {};
        vbView.BufferLocation = skinnedVertexResource_->GetGPUVirtualAddress() + sizeof(VertexData) * vertexOffset;

        vbView.SizeInBytes = static_cast<UINT>(sizeof(VertexData) * primitive.vertices.size());

        vbView.StrideInBytes = sizeof(VertexData);

        commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        commandList->IASetVertexBuffers(0, 1, &vbView);

        if (!primitive.indices.empty()) {
            commandList->IASetIndexBuffer(&primitive.ibView);
            commandList->DrawIndexedInstanced(static_cast<UINT>(primitive.indices.size()), 1, 0, 0, 0);
        } else {
            commandList->DrawInstanced(static_cast<UINT>(primitive.vertices.size()), 1, 0, 0);
        }

        vertexOffset += static_cast<uint32_t>(primitive.vertices.size());
    }
}
SkinningObject3d::~SkinningObject3d()
{
    if (transformationMatrixResource) {
        transformationMatrixResource->Unmap(0, nullptr);
    }

    if (materialResource) {
        materialResource->Unmap(0, nullptr);
    }
}
void SkinningObject3d::CreateSkinningResources()
{
    assert(skinningObject3dManager_);
    assert(skinningObject3dManager_->GetDxCommon());
    assert(model_);

    ID3D12Device* device = skinningObject3dManager_->GetDxCommon()->GetDevice();
    assert(device);

    uint32_t vertexCount = 0;
    for (const auto& primitive : model_->GetModelData().primitives) {
        vertexCount += static_cast<uint32_t>(primitive.vertices.size());
    }

    assert(vertexCount > 0);

    uint32_t bufferSize = sizeof(VertexData) * vertexCount;

    influenceResource_ = skinClusterData_.influenceResource;
    paletteResource_ = skinClusterData_.paletteResource;

    // =====================================================
    // 入力頂点バッファを自前で作る
    // 全 primitive の頂点を1本にまとめる
    // =====================================================
    inputVertexResource_ = skinningObject3dManager_->GetDxCommon()->CreateBufferResource(bufferSize);
    assert(inputVertexResource_);

    inputVertexResource_->SetName(L"SkinningObject3d::InputVertexResource");

    VertexData* mappedInputVertices = nullptr;
    HRESULT hr = inputVertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedInputVertices));
    assert(SUCCEEDED(hr));
    assert(mappedInputVertices);

    uint32_t copyOffset = 0;

    for (const auto& primitive : model_->GetModelData().primitives) {
        uint32_t primitiveVertexCount = static_cast<uint32_t>(primitive.vertices.size());

        for (uint32_t vertexIndex = 0; vertexIndex < primitiveVertexCount; ++vertexIndex) {
            mappedInputVertices[copyOffset + vertexIndex] = primitive.vertices[vertexIndex];
        }

        copyOffset += primitiveVertexCount;
    }

    assert(copyOffset == vertexCount);

    // =====================================================
    // Skinning結果を書き込む出力バッファ
    // =====================================================
    D3D12_HEAP_PROPERTIES heapProperties = {};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Width = bufferSize;
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    hr = device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(&skinnedVertexResource_));
    assert(SUCCEEDED(hr));
    assert(skinnedVertexResource_);

    skinnedVertexResource_->SetName(L"SkinningObject3d::SkinnedVertexResource");

    // =====================================================
    // 描画用 VBV
    // =====================================================
    skinnedVertexBufferView_.BufferLocation = skinnedVertexResource_->GetGPUVirtualAddress();
    skinnedVertexBufferView_.SizeInBytes = bufferSize;
    skinnedVertexBufferView_.StrideInBytes = sizeof(VertexData);

    // =====================================================
    // UAV
    // =====================================================
    skinnedVertexUavIndex_ = SrvManager::GetInstance()->Allocate();

    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = DXGI_FORMAT_UNKNOWN;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.FirstElement = 0;
    uavDesc.Buffer.NumElements = vertexCount;
    uavDesc.Buffer.StructureByteStride = sizeof(VertexData);
    uavDesc.Buffer.CounterOffsetInBytes = 0;
    uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

    D3D12_CPU_DESCRIPTOR_HANDLE uavHandle = SrvManager::GetInstance()->GetCPUDescriptorHandle(skinnedVertexUavIndex_);

    device->CreateUnorderedAccessView(
        skinnedVertexResource_.Get(),
        nullptr,
        &uavDesc,
        uavHandle);

    // =====================================================
    // input vertex SRV
    // =====================================================
    inputVertexSrvIndex_ = SrvManager::GetInstance()->Allocate();

    D3D12_SHADER_RESOURCE_VIEW_DESC inputSrvDesc = {};
    inputSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
    inputSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    inputSrvDesc.Buffer.FirstElement = 0;
    inputSrvDesc.Buffer.NumElements = vertexCount;
    inputSrvDesc.Buffer.StructureByteStride = sizeof(VertexData);
    inputSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
    inputSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    device->CreateShaderResourceView(
        inputVertexResource_.Get(),
        &inputSrvDesc,
        SrvManager::GetInstance()->GetCPUDescriptorHandle(inputVertexSrvIndex_));

    // =====================================================
    // influence SRV
    // =====================================================
    influenceSrvIndex_ = SrvManager::GetInstance()->Allocate();

    D3D12_SHADER_RESOURCE_VIEW_DESC influenceSrvDesc = {};
    influenceSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
    influenceSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    influenceSrvDesc.Buffer.FirstElement = 0;
    influenceSrvDesc.Buffer.NumElements = vertexCount;
    influenceSrvDesc.Buffer.StructureByteStride = sizeof(SkinCluster::VertexInfluence);
    influenceSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
    influenceSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    device->CreateShaderResourceView(
        influenceResource_.Get(),
        &influenceSrvDesc,
        SrvManager::GetInstance()->GetCPUDescriptorHandle(influenceSrvIndex_));

    // =====================================================
    // palette SRV
    // =====================================================
    paletteSrvIndex_ = SrvManager::GetInstance()->Allocate();

    D3D12_SHADER_RESOURCE_VIEW_DESC paletteSrvDesc = {};
    paletteSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
    paletteSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    paletteSrvDesc.Buffer.FirstElement = 0;
    paletteSrvDesc.Buffer.NumElements = static_cast<UINT>(playAnimation_->GetSkeleton()->joints.size());
    paletteSrvDesc.Buffer.StructureByteStride = sizeof(SkinCluster::WellForGPU);
    paletteSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
    paletteSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    device->CreateShaderResourceView(
        paletteResource_.Get(),
        &paletteSrvDesc,
        SrvManager::GetInstance()->GetCPUDescriptorHandle(paletteSrvIndex_));
}

void SkinningObject3d::DispatchSkinning()
{
    assert(skinningObject3dManager_);
    assert(skinningObject3dManager_->GetDxCommon());
    assert(skinnedVertexResource_);

    ID3D12GraphicsCommandList* commandList = skinningObject3dManager_->GetDxCommon()->GetCommandList();
    assert(commandList);

    uint32_t vertexCount = 0;
    for (const auto& primitive : model_->GetModelData().primitives) {
        vertexCount += static_cast<uint32_t>(primitive.vertices.size());
    }

    // =========================================
    // UAVに遷移
    // =========================================
    D3D12_RESOURCE_BARRIER barrierBefore = {};
    barrierBefore.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrierBefore.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrierBefore.Transition.pResource = skinnedVertexResource_.Get();
    barrierBefore.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
    barrierBefore.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    barrierBefore.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    commandList->ResourceBarrier(1, &barrierBefore);

    // =========================================
    // Compute設定
    // =========================================
    commandList->SetPipelineState(skinningObject3dManager_->GetComputePipelineState());
    commandList->SetComputeRootSignature(skinningObject3dManager_->GetComputeRootSignature());
    ID3D12DescriptorHeap* descriptorHeaps[] = { SrvManager::GetInstance()->GetDescriptorHeap() };
    commandList->SetDescriptorHeaps(1, descriptorHeaps);
    // =========================================
    // SRV / UAV セット
    // t0 : input vertex
    // t1 : influence
    // t2 : palette
    // u0 : output skinned vertex
    // =========================================

    commandList->SetComputeRootDescriptorTable(0, SrvManager::GetInstance()->GetGPUDescriptorHandle(paletteSrvIndex_));

    commandList->SetComputeRootDescriptorTable(1, SrvManager::GetInstance()->GetGPUDescriptorHandle(inputVertexSrvIndex_));

    commandList->SetComputeRootDescriptorTable(2, SrvManager::GetInstance()->GetGPUDescriptorHandle(influenceSrvIndex_));

    commandList->SetComputeRootDescriptorTable(3, SrvManager::GetInstance()->GetGPUDescriptorHandle(skinnedVertexUavIndex_));

    commandList->SetComputeRootConstantBufferView(4, skinningInformationResource_->GetGPUVirtualAddress());
    // =========================================
    // Dispatch
    // numthreads(1024,1,1) 前提
    // =========================================
    uint32_t threadGroupSizeX = 1024;
    uint32_t dispatchCountX = (vertexCount + threadGroupSizeX - 1) / threadGroupSizeX;

    commandList->Dispatch(dispatchCountX, 1, 1);

    // =========================================
    // UAVバリア
    // =========================================
    D3D12_RESOURCE_BARRIER uavBarrier = {};
    uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    uavBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    uavBarrier.UAV.pResource = skinnedVertexResource_.Get();

    commandList->ResourceBarrier(1, &uavBarrier);

    // =========================================
    // Drawで使える状態に戻す
    // =========================================
    D3D12_RESOURCE_BARRIER barrierAfter = {};
    barrierAfter.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrierAfter.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrierAfter.Transition.pResource = skinnedVertexResource_.Get();
    barrierAfter.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    barrierAfter.Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    barrierAfter.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    commandList->ResourceBarrier(1, &barrierAfter);
}

#pragma endregion

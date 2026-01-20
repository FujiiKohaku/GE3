#include "Model.h"
#include "../math/Object3DStruct.h"
#include "DirectXCommon.h"
#include "ModelCommon.h"
#include <algorithm>
#include <cassert>
#include <cstring>
// ===============================================
// モデル初期化処理
// ===============================================
void Model::Initialize(ModelCommon* modelCommon, const std::string& directorypath, const std::string& filename)
{
    modelCommon_ = modelCommon;
    modelData_ = Object3d::LoadModeFile(directorypath, filename);

    auto* dx = modelCommon_->GetDxCommon();

    for (MeshPrimitive& primitive : modelData_.primitives) {

        // ===== Vertex Buffer =====
        primitive.vertexResource = dx->CreateBufferResource(sizeof(VertexData) * primitive.vertices.size());

        primitive.vertexResource->SetName(L"MeshPrimitive::VertexBuffer");

        VertexData* vtx = nullptr;
        primitive.vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vtx));
        std::memcpy(vtx,
            primitive.vertices.data(),
            sizeof(VertexData) * primitive.vertices.size());

        primitive.vbView.BufferLocation = primitive.vertexResource->GetGPUVirtualAddress();
        primitive.vbView.SizeInBytes = UINT(sizeof(VertexData) * primitive.vertices.size());
        primitive.vbView.StrideInBytes = sizeof(VertexData);

        // ===== Index Buffer=====
        if (!primitive.indices.empty()) {
            primitive.indexResource = dx->CreateBufferResource(sizeof(uint32_t) * primitive.indices.size());

            uint32_t* idx = nullptr;
            primitive.indexResource->Map(0, nullptr, reinterpret_cast<void**>(&idx));
            std::memcpy(idx,
                primitive.indices.data(),
                sizeof(uint32_t) * primitive.indices.size());

            primitive.ibView.BufferLocation = primitive.indexResource->GetGPUVirtualAddress();
            primitive.ibView.SizeInBytes = UINT(sizeof(uint32_t) * primitive.indices.size());
            primitive.ibView.Format = DXGI_FORMAT_R32_UINT;
        }
    }

    // Texture
    TextureManager::GetInstance()->LoadTexture(modelData_.material.textureFilePath);
    modelData_.material.textureIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath(modelData_.material.textureFilePath);
}

// ===============================================
// モデル描画処理
// ===============================================
void Model::Draw()
{
    auto* commandList = modelCommon_->GetDxCommon()->GetCommandList();

    for (const MeshPrimitive& primitive : modelData_.primitives) {

        // topology
        commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        // vertex buffer
        commandList->IASetVertexBuffers(0, 1, &primitive.vbView);

        // texture
        auto handle = TextureManager::GetInstance()->GetSrvHandleGPU(modelData_.material.textureFilePath);
        commandList->SetGraphicsRootDescriptorTable(2, handle);

        // draw
        if (!primitive.indices.empty()) {
            commandList->IASetIndexBuffer(&primitive.ibView);
            commandList->DrawIndexedInstanced(UINT(primitive.indices.size()), 1, 0, 0, 0);
        } else {
            commandList->DrawInstanced(UINT(primitive.vertices.size()), 1, 0, 0);
        }
    }
}

SkinCluster Model::CreateSkinCluster(const Microsoft::WRL::ComPtr<ID3D12Device>& device, const Skeleton& skeleton, const ModelData& modelData, const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap, uint32_t descriptorSize)
{

    SkinCluster skinCluster;
    // palette用のResourceを確保
    skinCluster.paletteResource = DirectXCommon::GetInstance()->CreateBufferResource(sizeof(WellForGPU) * skeleton.joints.size());
    WellForGPU* mappedPalette = nullptr;
    skinCluster.paletteResource->Map(0, nullptr, reinterpret_cast<void**>(&mappedPalette));
    skinCluster.mappedPalette = { mappedPalette, skeleton.joints.size() }; // spanを使ってアクセスするようになる
    // 空きIndex確保
    skinCluster.paletteSrvIndex = SrvManager::GetInstance()->Allocate();
    skinCluster.paletteSrvHandle.first = DirectXCommon::GetInstance()->GetCPUDescriptorHandle(descriptorHeap, descriptorSize, skinCluster.paletteSrvIndex);
    skinCluster.paletteSrvHandle.second = DirectXCommon::GetInstance()->GetGPUDescriptorHandle(descriptorHeap, descriptorSize, skinCluster.paletteSrvIndex);
    // palette用の,srvを作成StructuredBufferでアクセスできるようにする
    D3D12_SHADER_RESOURCE_VIEW_DESC paletteSrvDesc {};
    paletteSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
    paletteSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    paletteSrvDesc.Buffer.FirstElement = 0;
    paletteSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
    paletteSrvDesc.Buffer.NumElements = UINT(skeleton.joints.size());
    paletteSrvDesc.Buffer.StructureByteStride = sizeof(WellForGPU);
    device->CreateShaderResourceView(skinCluster.paletteResource.Get(), &paletteSrvDesc, skinCluster.paletteSrvHandle.first);

    // influence用のResourceを確保

    skinCluster.influenceResource = DirectXCommon::GetInstance()->CreateBufferResource(sizeof(VertexInfluence) * modelData.primitives.size());
    VertexInfluence* mappedInfluence = nullptr;
    skinCluster.influenceResource->Map(0, nullptr, reinterpret_cast<void**>(&mappedInfluence));
    std::memset(mappedInfluence, 0, sizeof(VertexInfluence) * modelData.primitives.size()); // 0埋め weightを0にしておく
    skinCluster.mappedInfluence = { mappedInfluence, modelData.primitives.size() };

    // Influence用のVBVを作成
    skinCluster.influenceBufferView.BufferLocation = skinCluster.influenceResource->GetGPUVirtualAddress();
    skinCluster.influenceBufferView.SizeInBytes = UINT(sizeof(VertexInfluence) * modelData.primitives.size());
    skinCluster.influenceBufferView.StrideInBytes = sizeof(VertexInfluence);

    // InverseBindPoseMatrixの保存領域を作成
    skinCluster.inverseBindPoseMatrices.resize(skeleton.joints.size());
    std::generate(skinCluster.inverseBindPoseMatrices.begin(), skinCluster.inverseBindPoseMatrices.end(), MatrixMath::MakeIdentity4x4());
    // ModelDataのSkinCluster情報を解析してInfluenceの中身を埋める
    // modelData.skinClusterData : map<string, JointWeightData>
    for (const auto& jointWeight : modelData.skinClusterData) {

        // joint名 → skeleton内のjointIndex
        auto it = skeleton.jointMap.find(jointWeight.first);
        if (it == skeleton.jointMap.end()) {
            continue; // skeletonに存在しないjointは無視
        }
        const uint32_t jointIndex = it->second;

        // InverseBindPoseMatrix を保存
        skinCluster.inverseBindPoseMatrices[jointIndex] = jointWeight.second.inverseBindPoseMatrix;

        // 頂点ウェイトを Influence に詰める（最大4本）
        for (const auto& vertexWeight : jointWeight.second.vertexWeights) {

            VertexInfluence& influence = skinCluster.mappedInfluence[vertexWeight.vertexIndex];

            for (uint32_t i = 0; i < kNumMaxInfluence; ++i) {
                if (influence.weights[i] == 0.0f) {
                    influence.weights[i] = vertexWeight.weight;
                    influence.jointIndices[i] = jointIndex;
                    break;
                }
            }
        }
    }

    return skinCluster;
}

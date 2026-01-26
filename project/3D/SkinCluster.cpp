#include "SkinCluster.h"

SkinCluster::SkinClusterData SkinCluster::CreateSkinCluster(
    ID3D12Device* device,
    const Skeleton& skeleton,
    const ModelData& modelData,
    SrvManager* srvManager)
{
    SkinClusterData skinCluster;

    // =================================================
    // 入力チェック
    // =================================================
    assert(device);
    assert(srvManager);
    assert(!skeleton.joints.empty());
    assert(!modelData.primitives.empty());

    // =================================================
    // MatrixPalette（WellForGPU）
    // =================================================
    const size_t jointCount = skeleton.joints.size();
    assert(jointCount > 0);

    skinCluster.paletteResource = DirectXCommon::GetInstance()->CreateBufferResource(
        sizeof(WellForGPU) * jointCount);

    assert(skinCluster.paletteResource);

 WellForGPU* mappedPalette = nullptr;

    skinCluster.paletteResource->Map(
        0,
        nullptr,
        reinterpret_cast<void**>(&mappedPalette));

    // span を「作る」
    skinCluster.mappedPalette = std::span<WellForGPU>(
        mappedPalette,
        jointCount);

    uint32_t srvIndex = srvManager->Allocate();

    skinCluster.paletteSrvHandle.first = srvManager->GetCPUDescriptorHandle(srvIndex);
    skinCluster.paletteSrvHandle.second = srvManager->GetGPUDescriptorHandle(srvIndex);

    D3D12_SHADER_RESOURCE_VIEW_DESC paletteSrvDesc {};
    paletteSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
    paletteSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    paletteSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    paletteSrvDesc.Buffer.FirstElement = 0;
    paletteSrvDesc.Buffer.NumElements = static_cast<UINT>(jointCount);
    paletteSrvDesc.Buffer.StructureByteStride = sizeof(WellForGPU);
    paletteSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

    device->CreateShaderResourceView(
        skinCluster.paletteResource.Get(),
        &paletteSrvDesc,
        skinCluster.paletteSrvHandle.first);

    // =================================================
    // Influence（VertexInfluence）
    // =================================================
    size_t vertexCount = 0;
    for (const auto& primitive : modelData.primitives) {
        assert(!primitive.vertices.empty());
        vertexCount += primitive.vertices.size();
    }

    assert(vertexCount > 0);

    skinCluster.influenceResource = DirectXCommon::GetInstance()->CreateBufferResource(
        sizeof(VertexInfluence) * vertexCount);

    assert(skinCluster.influenceResource);

    VertexInfluence* mappedInfluence = nullptr;
    skinCluster.influenceResource->Map(
        0, nullptr, reinterpret_cast<void**>(&mappedInfluence));

    assert(mappedInfluence);

    std::memset(mappedInfluence,0,sizeof(VertexInfluence) * vertexCount);

    skinCluster.mappedInfluence = { mappedInfluence, vertexCount };

    assert(skinCluster.mappedInfluence.size() == vertexCount);

    skinCluster.influenceBufferView.BufferLocation = skinCluster.influenceResource->GetGPUVirtualAddress();
    skinCluster.influenceBufferView.SizeInBytes = static_cast<UINT>(sizeof(VertexInfluence) * vertexCount);
    skinCluster.influenceBufferView.StrideInBytes = sizeof(VertexInfluence);

    // =================================================
    // InverseBindPoseMatrix
    // =================================================
    skinCluster.inverseBindPoseMatrices.resize(jointCount);

    assert(skinCluster.inverseBindPoseMatrices.size() == jointCount);

    for (auto& m : skinCluster.inverseBindPoseMatrices) {
        m = MatrixMath::MakeIdentity4x4();
    }

    // =================================================
    // ModelData を解析して Influence を詰める
    // =================================================
    size_t baseVertex = 0;

    for (const auto& primitive : modelData.primitives) {

        assert(baseVertex + primitive.vertices.size() <= vertexCount);

        for (const auto& jointWeight : modelData.skinClusterData) {

            auto it = skeleton.jointMap.find(jointWeight.first);
            if (it == skeleton.jointMap.end()) {
                continue;
            }

            uint32_t jointIndex = it->second;

            assert(jointIndex < jointCount);

            skinCluster.inverseBindPoseMatrices[jointIndex] = jointWeight.second.inverseBindPoseMatrix;

            for (const auto& vertexWeight : jointWeight.second.vertexWeights) {

                size_t finalIndex = baseVertex + vertexWeight.vertexIndex;

                assert(finalIndex < skinCluster.mappedInfluence.size());

                VertexInfluence& influence = skinCluster.mappedInfluence[finalIndex];

                bool written = false;

                for (uint32_t i = 0; i < kNumMaxInfluence; ++i) {
                    if (influence.weights[i] == 0.0f) {
                        influence.weights[i] = vertexWeight.weight;
                        influence.jointIndices[i] = jointIndex;
                        written = true;
                        break;
                    }
                }

                assert(written && "VertexInfluence overflow");
            }
        }

        baseVertex += primitive.vertices.size();
    }

    assert(baseVertex == vertexCount);

    return skinCluster;
}


void SkinCluster::UpdateSkinCluster(SkinClusterData& skinCluster, const Skeleton& skeleton)
{
    for (size_t jointIndex = 0; jointIndex < skeleton.joints.size(); ++jointIndex) {

      
        
        skinCluster.mappedPalette[jointIndex].skeletonSpaceMatrix = MatrixMath::Multiply(skinCluster.inverseBindPoseMatrices[jointIndex], skeleton.joints[jointIndex].skeletonSpaceMatrix);

        // 法線用：逆転置行列
        skinCluster.mappedPalette[jointIndex].skeletonSpaceInverseTransposeMatrix = MatrixMath::Transpose(
            MatrixMath::Inverse(skinCluster.mappedPalette[jointIndex].skeletonSpaceMatrix));
    }
}

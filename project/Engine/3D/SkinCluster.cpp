#include "SkinCluster.h"

SkinCluster::SkinClusterData SkinCluster::CreateSkinCluster(
    ID3D12Device* device,
    const Skeleton& skeleton,
    const ModelData& modelData)
{
    SkinClusterData skinCluster;

    // =================================================
    // 入力チェック
    // =================================================
    assert(device);
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
    for (const auto& jointWeight : modelData.skinClusterData) {

        auto it = skeleton.jointMap.find(jointWeight.first);
        if (it == skeleton.jointMap.end()) {
            continue;
        }

        uint32_t jointIndex = it->second;

        assert(jointIndex < jointCount);

        skinCluster.inverseBindPoseMatrices[jointIndex] = jointWeight.second.inverseBindPoseMatrix;

        for (const auto& vertexWeight : jointWeight.second.vertexWeights) {

            size_t finalIndex = vertexWeight.vertexIndex;

            assert(finalIndex < skinCluster.mappedInfluence.size());

            VertexInfluence& influence = skinCluster.mappedInfluence[finalIndex];

            for (uint32_t i = 0; i < kNumMaxInfluence; ++i) {
                if (influence.weights[i] == 0.0f) {
                    influence.weights[i] = vertexWeight.weight;
                    influence.jointIndices[i] = jointIndex;
                    break;
                }
            }
        }
    }

    // ウェイトの合計が1.0になるように再正規化する
    for (size_t i = 0; i < vertexCount; ++i) {
        VertexInfluence& influence = skinCluster.mappedInfluence[i];
        float sum = 0.0f;
        for (uint32_t j = 0; j < kNumMaxInfluence; ++j) {
            sum += influence.weights[j];
        }
        if (sum > 0.0f) {
            for (uint32_t j = 0; j < kNumMaxInfluence; ++j) {
                influence.weights[j] /= sum;
            }
        }
    }

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

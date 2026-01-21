#pragma once
#include "../DirectXCommon/DirectXCommon.h"
#include "../Skeleton/Skeleton.h"
#include "../math/MathStruct.h"
#include "../math/MatrixMath.h"
#include <array> // std::array を使うなら（VertexInfluenceで使う想定）
#include <cstdint> // uint32_t / int32_t を使うなら
#include <d3d12.h>
#include <iostream>
#include <span> // std::span
#include <utility> // std::pair
#include <vector>
#include <wrl.h>
#include"SrvManager.h"
class SkinCluster {
public:
    SkinCluster() = default;
    ~SkinCluster() = default;

    

    static constexpr uint32_t kNumMaxInfluence = 4;

    struct VertexInfluence {
        std::array<float, kNumMaxInfluence> weights;
        std::array<int32_t, kNumMaxInfluence> jointIndices;
    };

    struct WellForGPU {
        Matrix4x4 skeletonSpaceMatrix;
        Matrix4x4 skeletonSpaceInverseTransposeMatrix;
    };
    struct SkinClusterData {
        // JointIndex順の InverseBindPoseMatrix
        std::vector<Matrix4x4> inverseBindPoseMatrices;

        // Influence
        Microsoft::WRL::ComPtr<ID3D12Resource> influenceResource;
        D3D12_VERTEX_BUFFER_VIEW influenceBufferView {};
        std::span<VertexInfluence> mappedInfluence;

        // MatrixPalette
        Microsoft::WRL::ComPtr<ID3D12Resource> paletteResource;
        std::span<WellForGPU> mappedPalette;
        std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE> paletteSrvHandle {};
    };
   static SkinClusterData CreateSkinCluster(ID3D12Device* device, const Skeleton& skeleton, const ModelData& modelData, SrvManager* srvManager);
    static void UpdateSkinCluster(SkinClusterData& skinCluster, const Skeleton& skeleton);

private:
};

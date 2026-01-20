#pragma once
#include "../Skeleton/Skeleton.h"
#include "../math/MatrixMath.h"
#include "../math/MatrixMath.h"
#include "../math/Object3DStruct.h"
#include <d3d12.h>
#include <span>
#include <vector>
#include <wrl.h>
struct SkinCluster {
    // CPU側
    std::vector<Matrix4x4> inverseBindPoseMatrices;

    // Influence
    Microsoft::WRL::ComPtr<ID3D12Resource> influenceResource;
    D3D12_VERTEX_BUFFER_VIEW influenceBufferView {};
    std::span<VertexInfluence> mappedInfluence;

    // Palette
    Microsoft::WRL::ComPtr<ID3D12Resource> paletteResource;
    std::span<WellForGPU> mappedPalette;
    uint32_t paletteSrvIndex = 0;
    std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE> paletteSrvHandle;
    // 毎フレーム更新
    void Update(const Skeleton& skeleton);
};

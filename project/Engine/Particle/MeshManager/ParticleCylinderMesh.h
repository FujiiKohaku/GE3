#pragma once

#include <cstdint>
#include <d3d12.h>
#include <vector>
#include <wrl.h>
#include "Engine/math/Object3DStruct.h"


class DirectXCommon;

class ParticleCylinderMesh {
public:
    void Initialize(DirectXCommon* dxCommon);

    const D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferView() const { return vertexBufferView_; }
    const D3D12_INDEX_BUFFER_VIEW& GetIndexBufferView() const { return indexBufferView_; }
    uint32_t GetIndexCount() const { return indexCount_; }

private:
    void CreateCylinderMesh();

private:
    DirectXCommon* dxCommon_ = nullptr;

    std::vector<VertexData> vertices_;
    std::vector<uint32_t> indices_;
    uint32_t indexCount_ = 0;

    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;

    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_ {};
    D3D12_INDEX_BUFFER_VIEW indexBufferView_ {};
};
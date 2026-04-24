#pragma once
#include"../../math/Object3DStruct.h"
#include <cstdint>
#include <d3d12.h>
#include <wrl.h>

class DirectXCommon;

class ParticleBoardMesh {
public:
    void Initialize(DirectXCommon* dxCommon);

    const D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferView() const { return vertexBufferView_; }
    const D3D12_INDEX_BUFFER_VIEW& GetIndexBufferView() const { return indexBufferView_; }
    uint32_t GetIndexCount() const { return indexCount_; }

private:
    void CreateBoardMesh();

private:
    DirectXCommon* dxCommon_ = nullptr;

    VertexData vertices_[4] = {};
    uint32_t indexList_[6] = { 0, 1, 2, 0, 2, 3 };
    uint32_t indexCount_ = 0;

    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;

    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_ {};
    D3D12_INDEX_BUFFER_VIEW indexBufferView_ {};
};
#pragma once
#include"../math/Object3DStruct.h"
#include <d3d12.h>
#include <wrl.h>


class DirectXCommon;

class ParticleMeshManager {
public:
public:

    void Initialize(DirectXCommon* dxCommon);

    const D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferView() const { return vertexBufferView_; }
    const D3D12_INDEX_BUFFER_VIEW& GetIndexBufferView() const { return indexBufferView_; }

private:
    void CreateBoardMesh();

private:
    DirectXCommon* dxCommon_ = nullptr;

    VertexData vertices_[4] = {};
    uint32_t indexList_[6] = { 0, 1, 2, 0, 2, 3 };

    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;

    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_ {};
    D3D12_INDEX_BUFFER_VIEW indexBufferView_ {};
};
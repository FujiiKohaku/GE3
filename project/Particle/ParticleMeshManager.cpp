#include "ParticleMeshManager.h"
#include "DirectXCommon.h"
#include <cassert>
#include <cstring>

void ParticleMeshManager::Initialize(DirectXCommon* dxCommon)
{
    dxCommon_ = dxCommon;
    CreateBoardMesh();
}

void ParticleMeshManager::CreateBoardMesh()
{
    vertices_[0].position = { -0.5f, 0.5f, 0.0f, 1.0f };
    vertices_[0].texcoord = { 0.0f, 0.0f };
    vertices_[0].normal = { 0.0f, 0.0f, -1.0f };

    vertices_[1].position = { 0.5f, 0.5f, 0.0f, 1.0f };
    vertices_[1].texcoord = { 1.0f, 0.0f };
    vertices_[1].normal = { 0.0f, 0.0f, -1.0f };

    vertices_[2].position = { 0.5f, -0.5f, 0.0f, 1.0f };
    vertices_[2].texcoord = { 1.0f, 1.0f };
    vertices_[2].normal = { 0.0f, 0.0f, -1.0f };

    vertices_[3].position = { -0.5f, -0.5f, 0.0f, 1.0f };
    vertices_[3].texcoord = { 0.0f, 1.0f };
    vertices_[3].normal = { 0.0f, 0.0f, -1.0f };

    vertexResource_ = dxCommon_->CreateBufferResource(sizeof(vertices_));
    assert(vertexResource_);

    VertexData* vertexBufferData = nullptr;
    vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexBufferData));
    memcpy(vertexBufferData, vertices_, sizeof(vertices_));
    vertexResource_->Unmap(0, nullptr);

    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = sizeof(vertices_);
    vertexBufferView_.StrideInBytes = sizeof(VertexData);

    indexResource_ = dxCommon_->CreateBufferResource(sizeof(indexList_));
    assert(indexResource_);

    uint32_t* indexBufferData = nullptr;
    indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&indexBufferData));
    memcpy(indexBufferData, indexList_, sizeof(indexList_));
    indexResource_->Unmap(0, nullptr);

    indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
    indexBufferView_.Format = DXGI_FORMAT_R32_UINT;
    indexBufferView_.SizeInBytes = sizeof(indexList_);
}
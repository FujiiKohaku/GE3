#include "ParticleBoxMesh.h"
#include "Engine/DirectXCommon/DirectXCommon.h"
#include <cassert>
#include <cstring>

void ParticleBoxMesh::Initialize(DirectXCommon* dxCommon)
{
    dxCommon_ = dxCommon;
    CreateBoxMesh();
}

void ParticleBoxMesh::CreateBoxMesh()
{
    // 前面 (z = -0.5f)
    vertices_[0].position  = { -0.5f,  0.5f, -0.5f, 1.0f }; vertices_[0].texcoord  = { 0.0f, 0.0f }; vertices_[0].normal  = { 0.0f, 0.0f, -1.0f };
    vertices_[1].position  = {  0.5f,  0.5f, -0.5f, 1.0f }; vertices_[1].texcoord  = { 1.0f, 0.0f }; vertices_[1].normal  = { 0.0f, 0.0f, -1.0f };
    vertices_[2].position  = {  0.5f, -0.5f, -0.5f, 1.0f }; vertices_[2].texcoord  = { 1.0f, 1.0f }; vertices_[2].normal  = { 0.0f, 0.0f, -1.0f };
    vertices_[3].position  = { -0.5f, -0.5f, -0.5f, 1.0f }; vertices_[3].texcoord  = { 0.0f, 1.0f }; vertices_[3].normal  = { 0.0f, 0.0f, -1.0f };

    // 後面 (z = 0.5f)
    vertices_[4].position  = {  0.5f,  0.5f,  0.5f, 1.0f }; vertices_[4].texcoord  = { 0.0f, 0.0f }; vertices_[4].normal  = { 0.0f, 0.0f,  1.0f };
    vertices_[5].position  = { -0.5f,  0.5f,  0.5f, 1.0f }; vertices_[5].texcoord  = { 1.0f, 0.0f }; vertices_[5].normal  = { 0.0f, 0.0f,  1.0f };
    vertices_[6].position  = { -0.5f, -0.5f,  0.5f, 1.0f }; vertices_[6].texcoord  = { 1.0f, 1.0f }; vertices_[6].normal  = { 0.0f, 0.0f,  1.0f };
    vertices_[7].position  = {  0.5f, -0.5f,  0.5f, 1.0f }; vertices_[7].texcoord  = { 0.0f, 1.0f }; vertices_[7].normal  = { 0.0f, 0.0f,  1.0f };

    // 左面 (x = -0.5f)
    vertices_[8].position  = { -0.5f,  0.5f,  0.5f, 1.0f }; vertices_[8].texcoord  = { 0.0f, 0.0f }; vertices_[8].normal  = { -1.0f, 0.0f, 0.0f };
    vertices_[9].position  = { -0.5f,  0.5f, -0.5f, 1.0f }; vertices_[9].texcoord  = { 1.0f, 0.0f }; vertices_[9].normal  = { -1.0f, 0.0f, 0.0f };
    vertices_[10].position = { -0.5f, -0.5f, -0.5f, 1.0f }; vertices_[10].texcoord = { 1.0f, 1.0f }; vertices_[10].normal = { -1.0f, 0.0f, 0.0f };
    vertices_[11].position = { -0.5f, -0.5f,  0.5f, 1.0f }; vertices_[11].texcoord = { 0.0f, 1.0f }; vertices_[11].normal = { -1.0f, 0.0f, 0.0f };

    // 右面 (x = 0.5f)
    vertices_[12].position = {  0.5f,  0.5f, -0.5f, 1.0f }; vertices_[12].texcoord = { 0.0f, 0.0f }; vertices_[12].normal = { 1.0f, 0.0f, 0.0f };
    vertices_[13].position = {  0.5f,  0.5f,  0.5f, 1.0f }; vertices_[13].texcoord = { 1.0f, 0.0f }; vertices_[13].normal = { 1.0f, 0.0f, 0.0f };
    vertices_[14].position = {  0.5f, -0.5f,  0.5f, 1.0f }; vertices_[14].texcoord = { 1.0f, 1.0f }; vertices_[14].normal = { 1.0f, 0.0f, 0.0f };
    vertices_[15].position = {  0.5f, -0.5f, -0.5f, 1.0f }; vertices_[15].texcoord = { 0.0f, 1.0f }; vertices_[15].normal = { 1.0f, 0.0f, 0.0f };

    // 上面 (y = 0.5f)
    vertices_[16].position = { -0.5f,  0.5f,  0.5f, 1.0f }; vertices_[16].texcoord = { 0.0f, 0.0f }; vertices_[16].normal = { 0.0f, 1.0f, 0.0f };
    vertices_[17].position = {  0.5f,  0.5f,  0.5f, 1.0f }; vertices_[17].texcoord = { 1.0f, 0.0f }; vertices_[17].normal = { 0.0f, 1.0f, 0.0f };
    vertices_[18].position = {  0.5f,  0.5f, -0.5f, 1.0f }; vertices_[18].texcoord = { 1.0f, 1.0f }; vertices_[18].normal = { 0.0f, 1.0f, 0.0f };
    vertices_[19].position = { -0.5f,  0.5f, -0.5f, 1.0f }; vertices_[19].texcoord = { 0.0f, 1.0f }; vertices_[19].normal = { 0.0f, 1.0f, 0.0f };

    // 下面 (y = -0.5f)
    vertices_[20].position = { -0.5f, -0.5f, -0.5f, 1.0f }; vertices_[20].texcoord = { 0.0f, 0.0f }; vertices_[20].normal = { 0.0f, -1.0f, 0.0f };
    vertices_[21].position = {  0.5f, -0.5f, -0.5f, 1.0f }; vertices_[21].texcoord = { 1.0f, 0.0f }; vertices_[21].normal = { 0.0f, -1.0f, 0.0f };
    vertices_[22].position = {  0.5f, -0.5f,  0.5f, 1.0f }; vertices_[22].texcoord = { 1.0f, 1.0f }; vertices_[22].normal = { 0.0f, -1.0f, 0.0f };
    vertices_[23].position = { -0.5f, -0.5f,  0.5f, 1.0f }; vertices_[23].texcoord = { 0.0f, 1.0f }; vertices_[23].normal = { 0.0f, -1.0f, 0.0f };

    // インデックスの設定
    uint32_t indices[36] = {
        0, 1, 2, 0, 2, 3,       // 前面
        4, 5, 6, 4, 6, 7,       // 後面
        8, 9, 10, 8, 10, 11,    // 左面
        12, 13, 14, 12, 14, 15, // 右面
        16, 17, 18, 16, 18, 19, // 上面
        20, 21, 22, 20, 22, 23  // 下面
    };
    memcpy(indexList_, indices, sizeof(indices));

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

    indexCount_ = 36;
}

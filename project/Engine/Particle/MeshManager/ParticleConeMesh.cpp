#include "ParticleConeMesh.h"
#include "Engine/DirectXCommon/DirectXCommon.h"
#include <cassert>
#include <cstring>
#include <numbers>
#include <cmath>

void ParticleConeMesh::Initialize(DirectXCommon* dxCommon)
{
    dxCommon_ = dxCommon;
    CreateConeMesh();
}

void ParticleConeMesh::CreateConeMesh()
{
    const uint32_t sliceCount = 3; // 底面を三角形にすることで「三角錐」になる
    const float radius = 0.5f;
    const float height = 1.0f;

    // 頂部
    vertices_.push_back({ { 0.0f, height * 0.5f, 0.0f, 1.0f }, { 0.5f, 0.0f }, { 0.0f, 1.0f, 0.0f } });

    // 側面用の底面の周りの頂点
    for (uint32_t i = 0; i <= sliceCount; ++i) {
        float theta = 2.0f * std::numbers::pi_v<float> * static_cast<float>(i) / static_cast<float>(sliceCount);
        float x = radius * std::cos(theta);
        float z = radius * std::sin(theta);
        float y = -height * 0.5f;

        VertexData v;
        v.position = { x, y, z, 1.0f };
        v.texcoord = { static_cast<float>(i) / sliceCount, 1.0f };
        v.normal = { x / radius, 0.5f, z / radius }; // 簡易的な法線
        vertices_.push_back(v);
    }

    // 底面用の中心点
    uint32_t bottomCenterIndex = static_cast<uint32_t>(vertices_.size());
    vertices_.push_back({ { 0.0f, -height * 0.5f, 0.0f, 1.0f }, { 0.5f, 0.5f }, { 0.0f, -1.0f, 0.0f } });

    // 底面用の周りの頂点
    uint32_t bottomRingBaseIndex = static_cast<uint32_t>(vertices_.size());
    for (uint32_t i = 0; i <= sliceCount; ++i) {
        float theta = 2.0f * std::numbers::pi_v<float> * static_cast<float>(i) / static_cast<float>(sliceCount);
        float x = radius * std::cos(theta);
        float z = radius * std::sin(theta);
        float y = -height * 0.5f;

        VertexData v;
        v.position = { x, y, z, 1.0f };
        v.texcoord = { 0.5f + 0.5f * std::cos(theta), 0.5f + 0.5f * std::sin(theta) };
        v.normal = { 0.0f, -1.0f, 0.0f };
        vertices_.push_back(v);
    }

    // 側面のインデックス
    for (uint32_t i = 0; i < sliceCount; ++i) {
        indices_.push_back(0);
        indices_.push_back(i + 1);
        indices_.push_back(i + 2);
    }

    // 底面のインデックス
    for (uint32_t i = 0; i < sliceCount; ++i) {
        indices_.push_back(bottomCenterIndex);
        indices_.push_back(bottomRingBaseIndex + i + 1);
        indices_.push_back(bottomRingBaseIndex + i);
    }

    indexCount_ = static_cast<uint32_t>(indices_.size());

    // 頂点バッファの生成
    vertexResource_ = dxCommon_->CreateBufferResource(sizeof(VertexData) * vertices_.size());
    assert(vertexResource_);

    VertexData* vertexBufferData = nullptr;
    vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexBufferData));
    memcpy(vertexBufferData, vertices_.data(), sizeof(VertexData) * vertices_.size());
    vertexResource_->Unmap(0, nullptr);

    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = static_cast<UINT>(sizeof(VertexData) * vertices_.size());
    vertexBufferView_.StrideInBytes = sizeof(VertexData);

    // インデックスバッファの生成
    indexResource_ = dxCommon_->CreateBufferResource(sizeof(uint32_t) * indices_.size());
    assert(indexResource_);

    uint32_t* indexBufferData = nullptr;
    indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&indexBufferData));
    memcpy(indexBufferData, indices_.data(), sizeof(uint32_t) * indices_.size());
    indexResource_->Unmap(0, nullptr);

    indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
    indexBufferView_.Format = DXGI_FORMAT_R32_UINT;
    indexBufferView_.SizeInBytes = static_cast<UINT>(sizeof(uint32_t) * indices_.size());
}

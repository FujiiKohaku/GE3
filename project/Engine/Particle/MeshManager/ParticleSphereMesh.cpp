#include "ParticleSphereMesh.h"
#include "Engine/DirectXCommon/DirectXCommon.h"
#include <cassert>
#include <cstring>
#include <numbers>
#include <cmath>

void ParticleSphereMesh::Initialize(DirectXCommon* dxCommon)
{
    dxCommon_ = dxCommon;
    CreateSphereMesh();
}

void ParticleSphereMesh::CreateSphereMesh()
{
    const uint32_t sliceCount = 8;
    const uint32_t stackCount = 8;
    const float radius = 0.5f;

    // 北極点
    vertices_.push_back({ { 0.0f, radius, 0.0f, 1.0f }, { 0.5f, 0.0f }, { 0.0f, 1.0f, 0.0f } });

    for (uint32_t i = 1; i < stackCount; ++i) {
        float phi = std::numbers::pi_v<float> * static_cast<float>(i) / static_cast<float>(stackCount);
        float y = radius * std::cos(phi);
        float r = radius * std::sin(phi);

        for (uint32_t j = 0; j <= sliceCount; ++j) {
            float theta = 2.0f * std::numbers::pi_v<float> * static_cast<float>(j) / static_cast<float>(sliceCount);
            float x = r * std::cos(theta);
            float z = r * std::sin(theta);

            VertexData v;
            v.position = { x, y, z, 1.0f };
            v.texcoord = { static_cast<float>(j) / sliceCount, static_cast<float>(i) / stackCount };
            v.normal = { x / radius, y / radius, z / radius };
            vertices_.push_back(v);
        }
    }

    // 南極点
    vertices_.push_back({ { 0.0f, -radius, 0.0f, 1.0f }, { 0.5f, 1.0f }, { 0.0f, -1.0f, 0.0f } });

    // 北極のキャップ
    for (uint32_t j = 0; j < sliceCount; ++j) {
        indices_.push_back(0);
        indices_.push_back(j + 2);
        indices_.push_back(j + 1);
    }

    // 中間のスタック
    uint32_t baseIndex = 1;
    uint32_t ringVertexCount = sliceCount + 1;
    for (uint32_t i = 0; i < stackCount - 2; ++i) {
        for (uint32_t j = 0; j < sliceCount; ++j) {
            indices_.push_back(baseIndex + i * ringVertexCount + j);
            indices_.push_back(baseIndex + (i + 1) * ringVertexCount + j);
            indices_.push_back(baseIndex + (i + 1) * ringVertexCount + j + 1);

            indices_.push_back(baseIndex + i * ringVertexCount + j);
            indices_.push_back(baseIndex + (i + 1) * ringVertexCount + j + 1);
            indices_.push_back(baseIndex + i * ringVertexCount + j + 1);
        }
    }

    // 南極のキャップ
    uint32_t southPoleIndex = static_cast<uint32_t>(vertices_.size() - 1);
    uint32_t lastRingBaseIndex = southPoleIndex - ringVertexCount;
    for (uint32_t j = 0; j < sliceCount; ++j) {
        indices_.push_back(southPoleIndex);
        indices_.push_back(lastRingBaseIndex + j);
        indices_.push_back(lastRingBaseIndex + j + 1);
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

#include "ParticleCylinderMesh.h"
#include "Engine/DirectXCommon/DirectXCommon.h"
#include <cassert>
#include <cmath>
#include <numbers>

void ParticleCylinderMesh::Initialize(DirectXCommon* dxCommon)
{
    assert(dxCommon != nullptr);

    dxCommon_ = dxCommon;

    CreateCylinderMesh();

    vertexResource_ = dxCommon_->CreateBufferResource(sizeof(VertexData) * vertices_.size());
    VertexData* vertexData = nullptr;
    vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
    std::memcpy(vertexData, vertices_.data(), sizeof(VertexData) * vertices_.size());

    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = UINT(sizeof(VertexData) * vertices_.size());
    vertexBufferView_.StrideInBytes = sizeof(VertexData);

    indexResource_ = dxCommon_->CreateBufferResource(sizeof(uint32_t) * indices_.size());
    uint32_t* indexData = nullptr;
    indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&indexData));
    std::memcpy(indexData, indices_.data(), sizeof(uint32_t) * indices_.size());

    indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
    indexBufferView_.SizeInBytes = UINT(sizeof(uint32_t) * indices_.size());
    indexBufferView_.Format = DXGI_FORMAT_R32_UINT;

    indexCount_ = static_cast<uint32_t>(indices_.size());
}

void ParticleCylinderMesh::CreateCylinderMesh()
{
    const uint32_t kDivide = 32;
    const float kRadius = 1.0f;
    const float kHeight = 3.0f;

    const float radianPerDivide = 2.0f * std::numbers::pi_v<float> / static_cast<float>(kDivide);

    vertices_.clear();
    indices_.clear();

    for (uint32_t index = 0; index <= kDivide; index++) {
        float radian = static_cast<float>(index) * radianPerDivide;

        float sin = std::sin(radian);
        float cos = std::cos(radian);

        float u = static_cast<float>(index) / static_cast<float>(kDivide);

        VertexData topVertex {};
        topVertex.position = { -sin * kRadius, kHeight, cos * kRadius, 1.0f };
        topVertex.texcoord = { u, 0.0f };
        topVertex.normal = { -sin, 0.0f, cos };

        VertexData bottomVertex {};
        bottomVertex.position = { -sin * kRadius, 0.0f, cos * kRadius, 1.0f };
        bottomVertex.texcoord = { u, 1.0f };
        bottomVertex.normal = { -sin, 0.0f, cos };

        vertices_.push_back(topVertex);
        vertices_.push_back(bottomVertex);
    }

    for (uint32_t index = 0; index < kDivide; index++) {
        uint32_t top0 = index * 2;
        uint32_t bottom0 = top0 + 1;
        uint32_t top1 = top0 + 2;
        uint32_t bottom1 = top0 + 3;

        indices_.push_back(top0);
        indices_.push_back(top1);
        indices_.push_back(bottom0);

        indices_.push_back(bottom0);
        indices_.push_back(top1);
        indices_.push_back(bottom1);
    }
}
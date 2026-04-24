#include "ParticleRingMesh.h"
#include "Engine/DirectXCommon/DirectXCommon.h"
#include <cassert>
#include <cmath>
#include <cstring>

void ParticleRingMesh::Initialize(DirectXCommon* dxCommon)
{
    dxCommon_ = dxCommon;
    CreateRingMesh();
}

void ParticleRingMesh::CreateRingMesh()
{
    const uint32_t ringDivide = 32;
    const float outerRadius = 1.0f;
    const float innerRadius = 0.5f;
    const float pi = 3.141592f;
    const float radianStep = 2.0f * pi / float(ringDivide);

    vertices_.clear();
    indices_.clear();

    for (uint32_t index = 0; index < ringDivide; ++index) {
        float angle = float(index) * radianStep;
        float nextAngle = float(index + 1) * radianStep;

        float sinValue = std::sin(angle);
        float cosValue = std::cos(angle);
        float nextSinValue = std::sin(nextAngle);
        float nextCosValue = std::cos(nextAngle);

        float u = float(index) / float(ringDivide);
        float nextU = float(index + 1) / float(ringDivide);

        VertexData outerVertex;
        outerVertex.position = { -sinValue * outerRadius, cosValue * outerRadius, 0.0f, 1.0f };
        outerVertex.texcoord = { u, 0.0f };
        outerVertex.normal = { 0.0f, 0.0f, 1.0f };

        VertexData nextOuterVertex;
        nextOuterVertex.position = { -nextSinValue * outerRadius, nextCosValue * outerRadius, 0.0f, 1.0f };
        nextOuterVertex.texcoord = { nextU, 0.0f };
        nextOuterVertex.normal = { 0.0f, 0.0f, 1.0f };

        VertexData innerVertex;
        innerVertex.position = { -sinValue * innerRadius, cosValue * innerRadius, 0.0f, 1.0f };
        innerVertex.texcoord = { u, 1.0f };
        innerVertex.normal = { 0.0f, 0.0f, 1.0f };

        VertexData nextInnerVertex;
        nextInnerVertex.position = { -nextSinValue * innerRadius, nextCosValue * innerRadius, 0.0f, 1.0f };
        nextInnerVertex.texcoord = { nextU, 1.0f };
        nextInnerVertex.normal = { 0.0f, 0.0f, 1.0f };

        uint32_t baseIndex = (uint32_t)vertices_.size();

        vertices_.push_back(outerVertex);
        vertices_.push_back(nextOuterVertex);
        vertices_.push_back(innerVertex);
        vertices_.push_back(nextInnerVertex);

        indices_.push_back(baseIndex + 0);
        indices_.push_back(baseIndex + 2);
        indices_.push_back(baseIndex + 1);

        indices_.push_back(baseIndex + 2);
        indices_.push_back(baseIndex + 3);
        indices_.push_back(baseIndex + 1);
    }

    indexCount_ = (uint32_t)indices_.size();

    vertexResource_ = dxCommon_->CreateBufferResource(sizeof(VertexData) * vertices_.size());
    assert(vertexResource_);

    VertexData* vertexBufferData = nullptr;
    vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexBufferData));
    memcpy(vertexBufferData, vertices_.data(), sizeof(VertexData) * vertices_.size());
    vertexResource_->Unmap(0, nullptr);

    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = UINT(sizeof(VertexData) * vertices_.size());
    vertexBufferView_.StrideInBytes = sizeof(VertexData);

    indexResource_ = dxCommon_->CreateBufferResource(sizeof(uint32_t) * indices_.size());
    assert(indexResource_);

    uint32_t* indexBufferData = nullptr;
    indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&indexBufferData));
    memcpy(indexBufferData, indices_.data(), sizeof(uint32_t) * indices_.size());
    indexResource_->Unmap(0, nullptr);

    indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
    indexBufferView_.Format = DXGI_FORMAT_R32_UINT;
    indexBufferView_.SizeInBytes = UINT(sizeof(uint32_t) * indices_.size());
}
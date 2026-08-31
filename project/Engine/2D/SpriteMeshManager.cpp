#include "SpriteMeshManager.h"
#include "Engine/DirectXCommon/DirectXCommon.h"
#include "Engine/math/SpriteStruct.h"
#include <cassert>
#include <cstring>
#include <vector>

void SpriteMeshManager::Initialize(DirectXCommon* dxCommon)
{
    assert(dxCommon);
    dxCommon_ = dxCommon;

    SpriteMeshDesc quadDesc;
    GetOrCreateMesh(quadDesc);
}

const SpriteMesh& SpriteMeshManager::GetOrCreateMesh(const SpriteMeshDesc& desc)
{
    const SpriteMeshDesc normalizedDesc = NormalizeDesc(desc);
    const std::string cacheKey = MakeMeshCacheKey(normalizedDesc);
    std::unordered_map<std::string, SpriteMesh>::iterator iterator = meshCache_.find(cacheKey);
    if (iterator != meshCache_.end()) {
        return iterator->second;
    }

    SpriteMesh mesh = CreateMesh(normalizedDesc);
    std::pair<std::unordered_map<std::string, SpriteMesh>::iterator, bool> result =
        meshCache_.emplace(cacheKey, std::move(mesh));
    return result.first->second;
}

SpriteMesh SpriteMeshManager::CreateMesh(const SpriteMeshDesc& desc)
{
    const uint32_t vertexCountX = desc.divisionX + 1;
    const uint32_t vertexCountY = desc.divisionY + 1;
    std::vector<SpriteVertexData> vertices;
    vertices.reserve(static_cast<size_t>(vertexCountX) * vertexCountY);

    for (uint32_t y = 0; y < vertexCountY; ++y) {
        const float normalizedY = static_cast<float>(y) / static_cast<float>(desc.divisionY);
        for (uint32_t x = 0; x < vertexCountX; ++x) {
            const float normalizedX = static_cast<float>(x) / static_cast<float>(desc.divisionX);
            SpriteVertexData vertex;
            vertex.position = { normalizedX, normalizedY, 0.0f, 1.0f };
            vertex.texcoord = { normalizedX, normalizedY };
            vertices.push_back(vertex);
        }
    }

    std::vector<uint32_t> indices;
    indices.reserve(static_cast<size_t>(desc.divisionX) * desc.divisionY * 6);
    for (uint32_t y = 0; y < desc.divisionY; ++y) {
        for (uint32_t x = 0; x < desc.divisionX; ++x) {
            const uint32_t topLeft = y * vertexCountX + x;
            const uint32_t topRight = topLeft + 1;
            const uint32_t bottomLeft = (y + 1) * vertexCountX + x;
            const uint32_t bottomRight = bottomLeft + 1;

            indices.push_back(bottomLeft);
            indices.push_back(topLeft);
            indices.push_back(bottomRight);
            indices.push_back(topLeft);
            indices.push_back(topRight);
            indices.push_back(bottomRight);
        }
    }

    SpriteMesh mesh;
    mesh.vertexResource = dxCommon_->CreateBufferResource(sizeof(SpriteVertexData) * vertices.size());
    mesh.indexResource = dxCommon_->CreateBufferResource(sizeof(uint32_t) * indices.size());
    mesh.vertexResource->SetName(L"SpriteMeshManager::VertexBuffer");
    mesh.indexResource->SetName(L"SpriteMeshManager::IndexBuffer");

    SpriteVertexData* mappedVertices = nullptr;
    uint32_t* mappedIndices = nullptr;
    mesh.vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&mappedVertices));
    mesh.indexResource->Map(0, nullptr, reinterpret_cast<void**>(&mappedIndices));
    std::memcpy(mappedVertices, vertices.data(), sizeof(SpriteVertexData) * vertices.size());
    std::memcpy(mappedIndices, indices.data(), sizeof(uint32_t) * indices.size());
    mesh.vertexResource->Unmap(0, nullptr);
    mesh.indexResource->Unmap(0, nullptr);

    mesh.vertexBufferView.BufferLocation = mesh.vertexResource->GetGPUVirtualAddress();
    mesh.vertexBufferView.SizeInBytes = static_cast<UINT>(sizeof(SpriteVertexData) * vertices.size());
    mesh.vertexBufferView.StrideInBytes = sizeof(SpriteVertexData);

    mesh.indexBufferView.BufferLocation = mesh.indexResource->GetGPUVirtualAddress();
    mesh.indexBufferView.SizeInBytes = static_cast<UINT>(sizeof(uint32_t) * indices.size());
    mesh.indexBufferView.Format = DXGI_FORMAT_R32_UINT;
    mesh.indexCount = static_cast<uint32_t>(indices.size());
    return mesh;
}

SpriteMeshDesc SpriteMeshManager::NormalizeDesc(const SpriteMeshDesc& desc) const
{
    SpriteMeshDesc normalizedDesc = desc;
    if (normalizedDesc.divisionX == 0) {
        normalizedDesc.divisionX = 1;
    }
    if (normalizedDesc.divisionY == 0) {
        normalizedDesc.divisionY = 1;
    }
    return normalizedDesc;
}

std::string SpriteMeshManager::MakeMeshCacheKey(const SpriteMeshDesc& desc) const
{
    std::string cacheKey = std::to_string(desc.divisionX);
    cacheKey += "x";
    cacheKey += std::to_string(desc.divisionY);
    return cacheKey;
}

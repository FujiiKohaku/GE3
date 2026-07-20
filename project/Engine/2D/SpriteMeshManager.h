#pragma once

#include <cstdint>
#include <d3d12.h>
#include <string>
#include <unordered_map>
#include <wrl.h>

class DirectXCommon;

struct SpriteMeshDesc {
    uint32_t divisionX = 1;
    uint32_t divisionY = 1;
};

struct SpriteMesh {
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexResource;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView = {};
    D3D12_INDEX_BUFFER_VIEW indexBufferView = {};
    uint32_t indexCount = 0;
};

class SpriteMeshManager {
public:
    void Initialize(DirectXCommon* dxCommon);
    const SpriteMesh& GetOrCreateMesh(const SpriteMeshDesc& desc);

private:
    SpriteMesh CreateMesh(const SpriteMeshDesc& desc);
    SpriteMeshDesc NormalizeDesc(const SpriteMeshDesc& desc) const;
    std::string MakeMeshCacheKey(const SpriteMeshDesc& desc) const;

private:
    DirectXCommon* dxCommon_ = nullptr;
    std::unordered_map<std::string, SpriteMesh> meshCache_;
};

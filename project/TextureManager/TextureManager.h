#pragma once
#include "DirectXCommon.h"
#include "DirectXTex/DirectXTex.h"
#include "DirectXTex/d3dx12.h"
#include "SrvManager.h"
#include <algorithm>
#include <cassert>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <wrl.h>

class TextureManager {
public:
    // シングルトン
    static TextureManager* GetInstance();
    void Finalize();

    // 初期化・読み込み
    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);
    void LoadTexture(const std::string& filePath);

    // 取得
    uint32_t GetTextureIndexByFilePath(const std::string& filePath);
    D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandleGPU(const std::string& filePath);
    const DirectX::TexMetadata& GetMetaData(const std::string& filePath);

    struct TextureData {
        DirectX::TexMetadata metadata;
        Microsoft::WRL::ComPtr<ID3D12Resource> resource;
        uint32_t srvIndex = 0;
        D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU {};
        D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU {};
    };

    const TextureData* GetTextureData(const std::string& filePath) const
    {
        std::unordered_map<std::string, TextureData>::const_iterator it = textureDatas.find(filePath);
        if (it != textureDatas.end()) {
            return &it->second;
        }
        return nullptr;
    }

    class ConstructorKey {
    private:
        ConstructorKey() = default;
        friend class TextureManager;
    };

    TextureManager(ConstructorKey) { }
    ~TextureManager() = default;

private:
    TextureManager(const TextureManager&) = delete;
    TextureManager& operator=(const TextureManager&) = delete;

private:
    std::unordered_map<std::string, TextureData> textureDatas;

    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;

    static uint32_t kSRVIndexTop;
    static const uint32_t kMaxSRVCount = 512;

    uint32_t defaultTextureSrvIndex_ = 0;

    static std::unique_ptr<TextureManager> instance;
};
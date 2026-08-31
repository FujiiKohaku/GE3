#pragma once
#include "Engine/DirectXCommon/DirectXCommon.h"
#include "DirectXTex/DirectXTex.h"
#include "DirectXTex/d3dx12.h"
#include"Engine/SrvManager/SrvManager.h"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
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
    void LoadTextureFromMemory(const std::string& textureKey, const uint8_t* data, size_t dataSize);
    void LoadTextureFromBGRA(
        const std::string& textureKey,
        const uint8_t* data,
        size_t width,
        size_t height);
    void UpdateTextureFromBGRA(
        const std::string& textureKey,
        const uint8_t* data,
        size_t width,
        size_t height);
    // 保留中のテクスチャ転送をまとめてGPUへ送る。
    void FlushUploads();

    // 取得
    uint32_t GetTextureIndexByFilePath(const std::string& filePath);
    D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandleGPU(uint32_t textureHandle) const;
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
        ConstructorKey() = default;
        friend class TextureManager;
    };
    explicit TextureManager(ConstructorKey);

    ~TextureManager() = default;

private:
    TextureManager(const TextureManager&) = delete;
    TextureManager& operator=(const TextureManager&) = delete;

private:
    std::unordered_map<std::string, TextureData> textureDatas;

    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;

    static const uint32_t kMaxSRVCount = 512;

    uint32_t defaultTextureSrvIndex_ = 0;

    // GPU転送が完了するまでアップロード用リソースを保持する。
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> pendingUploadResources_;
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> pendingTextureResources_;

    void RegisterTexture(const std::string& textureKey, DirectX::ScratchImage& image);

    static std::unique_ptr<TextureManager> instance;
};

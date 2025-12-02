#pragma once
#include "DirectXCommon.h"
#include <wrl.h>

class SrvManager {
public:
    // Singleton: インスタンス取得
    static SrvManager* GetInstance();

    // 初期化
    void Initialize(DirectXCommon* dxCommon);

    void PreDraw();

    uint32_t Allocate();

    D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(uint32_t index);
    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(uint32_t index);

    ID3D12DescriptorHeap* GetDescriptorHeap() const
    {
        return descriptorHeap.Get();
    }

    void CreateSRVforTexture2D(uint32_t srvIndex, ID3D12Resource* pResource, DXGI_FORMAT Format, UINT MipLevels);
    void CreateSRVforStructuredBuffer(uint32_t srvIndex, ID3D12Resource* pResource, UINT numElements, UINT structureByteStride);

    void SetGraphicsRootDescriptorTable(UINT RootParameterIndex, uint32_t srvIndex);

    bool CanAllocate() const;

    static const uint32_t kMaxSRVCount;

private:
    // Singleton化要素
    SrvManager() = default;
    ~SrvManager() = default;

    SrvManager(const SrvManager&) = delete;
    SrvManager& operator=(const SrvManager&) = delete;

private:
    DirectXCommon* dxCommon_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap = nullptr;

    uint32_t descriptorSize = 0;
    uint32_t useIndex = 0;
};

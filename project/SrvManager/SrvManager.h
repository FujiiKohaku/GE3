#pragma once
#include "DirectXCommon.h"
#include <wrl.h>
class SrvManager {
public:
    // 初期化
    void Initialize(DirectXCommon* dxCommon);

    void PreDraw();

    // 次の空きスロットを1つ予約して、その番号を返す関数
    uint32_t Allocate();

    D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(uint32_t index);
    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(uint32_t index);

    // SRV生成(テクスチャ用)
    void CreateSRVforTexture2D(uint32_t srvIndex, ID3D12Resource* pResource, DXGI_FORMAT Format, UINT MipLevels);
    // SRV生成(バッファ用)
    void CreateSRVforStructuredBuffer(uint32_t srvIndex, ID3D12Resource* pResource, UINT numElements, UINT structureByteStride);
    // SRVセットコマンド
    void SetGraphicsRootDescriptorTable(UINT RootParameterIndex, uint32_t srvIndex);
    bool CanAllocate() const;
    // 最大SRV数(最大テクスチャ枚数)
    static const uint32_t kMaxSRVCount;

private:
    DirectXCommon* dxCommon_ = nullptr;


    // SRV用のヒープ
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap = nullptr;

    uint32_t descriptorSize = 0;

    // 次に使用するSRVのインデックス
    uint32_t useIndex = 0;
};

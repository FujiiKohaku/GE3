#pragma once
#include "Engine/DirectXCommon/DirectXCommon.h"

class PreviewTarget {
public:
    ~PreviewTarget();
    void Initialize(DirectXCommon* dx);
    // Call after the GPU fence, before recording commands using this texture.
    void Resize();
    void Begin();
    void End();
    D3D12_CPU_DESCRIPTOR_HANDLE GetRtv() const;
    D3D12_GPU_DESCRIPTOR_HANDLE GetSrv() const;
    ID3D12Resource* GetResource() const { return texture_.Get(); }
    uint32_t GetWidth() const { return width_; }
    uint32_t GetHeight() const { return height_; }
    void SavePng(const std::filesystem::path& path, ID3D12Resource* source = nullptr) const;

private:
    DirectXCommon* dx_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> texture_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap_;
    uint32_t srvIndex_ = UINT_MAX;
    uint32_t width_ = 1280;
    uint32_t height_ = 720;
};

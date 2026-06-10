#pragma once

#include <Windows.h>
#include <cstdint>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>

struct UIEditorTextureResource {
    Microsoft::WRL::ComPtr<ID3D12Resource> texture;
    Microsoft::WRL::ComPtr<ID3D12Resource> uploadBuffer;
    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = {};
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = {};
    int width = 0;
    int height = 0;
    bool isValid = false;
};

class UIEditorD3D12 {
public:
    UIEditorD3D12();
    ~UIEditorD3D12();

    bool Initialize(HWND hwnd, int width, int height);
    void Shutdown();

    void Resize(int width, int height);
    void BeginFrame();
    void EndFrame();
    void WaitForGpu();

    ID3D12Device* GetDevice() const;
    ID3D12GraphicsCommandList* GetCommandList() const;
    ID3D12DescriptorHeap* GetSrvDescriptorHeap() const;
    int GetWidth() const;
    int GetHeight() const;

    D3D12_CPU_DESCRIPTOR_HANDLE GetFontCpuHandle() const;
    D3D12_GPU_DESCRIPTOR_HANDLE GetFontGpuHandle() const;

    bool CreateTextureFromRGBA(
        const unsigned char* pixels,
        int width,
        int height,
        UIEditorTextureResource* textureResource);

private:
    D3D12_CPU_DESCRIPTOR_HANDLE GetRtvHandle(int index) const;
    D3D12_CPU_DESCRIPTOR_HANDLE GetSrvCpuHandle(int index) const;
    D3D12_GPU_DESCRIPTOR_HANDLE GetSrvGpuHandle(int index) const;
    int AllocateSrvDescriptor();

private:
    static const int kFrameCount = 2;
    static const int kSrvDescriptorCount = 128;

    HWND hwnd_ = nullptr;
    int width_ = 0;
    int height_ = 0;
    int currentBackBufferIndex_ = 0;
    int nextSrvIndex_ = 1;
    uint64_t fenceValue_ = 0;
    HANDLE fenceEvent_ = nullptr;

    Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory_;
    Microsoft::WRL::ComPtr<ID3D12Device> device_;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue_;
    Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvHeap_;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocators_[kFrameCount];
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> uploadCommandAllocator_;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> uploadCommandList_;
    Microsoft::WRL::ComPtr<ID3D12Resource> backBuffers_[kFrameCount];
    Microsoft::WRL::ComPtr<ID3D12Fence> fence_;

    UINT rtvDescriptorSize_ = 0;
    UINT srvDescriptorSize_ = 0;
};

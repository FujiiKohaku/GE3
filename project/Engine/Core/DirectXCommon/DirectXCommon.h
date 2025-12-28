#pragma once
#include "../../System/Logger/Logger.h"
#include "../../System/StringUtility/StringUtility.h"
#include "../Winapp/WinApp.h"
#include <Windows.h>
#include <array>
#include <cassert>
#include <d3d12.h>
#include <dxcapi.h>
#include <dxgi1_6.h>
#include <wrl.h>
#pragma comment(lib, "dxcompiler.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#include "DirectXTex/DirectXTex.h"
#include "DirectXTex/d3dx12.h"

class DirectXCommon {
public:
    // ===== Singleton =====
    static DirectXCommon* GetInstance();
    // インスタンス
    static DirectXCommon* instance;
    // DX初期化
    void Initialize(WinApp* winApp);

    // 描画前処理
    void PreDraw();
    // 描画後処理
    void PostDraw();
    static void Finalize();
    // Getter達
    ID3D12Device* GetDevice() const { return device.Get(); }
    ID3D12GraphicsCommandList* GetCommandList() const { return commandList.Get(); }
    ID3D12CommandAllocator* GetCommandAllocator() const { return commandAllocator.Get(); }
    ID3D12CommandQueue* GetCommandQueue() const { return commandQueue.Get(); }

    size_t GetSwapChainResourcesNum() const
    {
        return kSwapChainBufferCount;
    }

    Microsoft::WRL::ComPtr<IDxcBlob> CompileShader(const std::wstring& filepath, const wchar_t* profile);
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResource(size_t sizeInBytes);
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateTextureResource(Microsoft::WRL::ComPtr<ID3D12Device> device, const DirectX::TexMetadata& metadata);
    Microsoft::WRL::ComPtr<ID3D12Resource> UploadTextureData(Microsoft::WRL::ComPtr<ID3D12Resource> texture, const DirectX::ScratchImage& mipImages);

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible);

    static D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap, uint32_t descriptorSize, uint32_t index);

    static D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap, uint32_t descriptorSize, uint32_t index);

    void WaitForGPU();

private:
    // ===== Singleton化の基本 =====
    DirectXCommon() = default;
    ~DirectXCommon() = default;
    DirectXCommon(const DirectXCommon&) = delete;
    DirectXCommon& operator=(const DirectXCommon&) = delete;

private:
    // DX内部処理関数たち（あなたのコードそのまま）
    void InitializeDevice();
    void InitializeCommand();
    void InitializeSwapChain();
    void InitializeDepthBuffer();
    void InitializeDescriptorHeaps();
    void InitializeRenderTargetView();
    void InitializeDepthStencilView();
    void InitializeFence();
    void InitializeViewport();
    void InitializeScissorRect();
    void InitializeDxcCompiler();

private:
    Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Device> device = nullptr;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue = nullptr;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator = nullptr;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList = nullptr;
    Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain = nullptr;
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc {};
    Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilResource;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap = nullptr;
    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc {};
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[2];

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap = nullptr;

    uint32_t descriptorSizeRTV = 0;
    uint32_t descriptorSizeDSV = 0;
    static const uint32_t kSwapChainBufferCount = 2;

    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, 2> swapChainResources;
    Microsoft::WRL::ComPtr<ID3D12Fence> fence = nullptr;
    uint64_t fenceValue = 0;
    HANDLE fenceEvent = nullptr;

    WinApp* winApp_ = nullptr;
    D3D12_VIEWPORT viewport {};
    D3D12_RECT scissorRect {};

    Microsoft::WRL::ComPtr<IDxcUtils> dxcUtils;
    Microsoft::WRL::ComPtr<IDxcCompiler3> dxcCompiler;
    Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler;
};
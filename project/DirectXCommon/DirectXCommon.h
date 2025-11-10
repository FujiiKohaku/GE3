#pragma once
#include "Logger.h"
#include "StringUtility.h"
#include <WinApp.h>
#include <Windows.h>
#include <array>
#include <cassert>
#include <d3d12.h>
#include <dxcapi.h>
#include <dxgi1_6.h>
#include <wrl.h> // ComPtr 用
#pragma comment(lib, "dxcompiler.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#include "DirectXTex/DirectXTex.h"
#include "DirectXTex/d3dx12.h"
class DirectXCommon {
public:
    // 初期化
    void Initialize(WinApp* winApp);

    //// SRVの指定番号のCPUデスクリプタハンドルを取得する
    //D3D12_CPU_DESCRIPTOR_HANDLE GetSRVCPUDescriptorHandle(uint32_t index);

    //D3D12_GPU_DESCRIPTOR_HANDLE GetSRVGPUDescriptorHandle(uint32_t index);
    // 描画前の処理
    void PreDraw();
    // 描画後の処理
    void PostDraw();
 
    // Getter
    ID3D12Device* GetDevice() const { return device.Get(); }
    ID3D12GraphicsCommandList* GetCommandList() const { return commandList.Get(); }
    ID3D12CommandAllocator* GetCommandAllocator() const { return commandAllocator.Get(); }
    ID3D12CommandQueue* GetCommandQueue() const { return commandQueue.Get(); }
    // SRVヒープとサイズのGetter
    //Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> GetSRVDescriptorHeap() const { return srvDescriptorHeap; }
    //uint32_t GetSRVDescriptorSize() const { return descriptorSizeSRV; }
    // シェーダーコンパイル関数
    Microsoft::WRL::ComPtr<IDxcBlob> CompileShader(const std::wstring& filepath, const wchar_t* profile);
    // リソース生成関数
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResource(size_t sizeInBytes);
    // Textureリソース生成関数
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateTextureResource(Microsoft::WRL::ComPtr<ID3D12Device> device, const DirectX::TexMetadata& metadata);
    //// テクスチャファイルの読み込み関数
    // static DirectX::ScratchImage LoadTexture(const std::string& filePath);
    //  txtureデータ転送関数
    Microsoft::WRL::ComPtr<ID3D12Resource> UploadTextureData(Microsoft::WRL::ComPtr<ID3D12Resource> texture, const DirectX::ScratchImage& mipImages);

    // ディスクリプタヒープ生成関数
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisivle);

    // 指定番号のCPUディスクリプタハンドルを取得する関数
    static D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap, uint32_t descriptorSize, uint32_t index);
    // 指定番号のGPUディスクリプタハンドルを取得する関数
    static D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap, uint32_t descriptorSize, uint32_t index);

    void WaitForGPU();

private:
    // DXGIファクトリーの生成
    Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory = nullptr;
    // デバイス
    Microsoft::WRL::ComPtr<ID3D12Device> device = nullptr;

    // コマンドキュー
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue = nullptr;
    // コマンドアロケータ
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator = nullptr;
    // コマンドリスト
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList = nullptr;

    // スワップチェイン
    Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain = nullptr;
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc {};
    // 深度バッファリソース
    Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilResource;

    // RTV用のヒープでディスクリプタの数は２。RTVはSHADER内で触るものではないので、shaderVisivleはfalse02_02
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap = nullptr;
    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc {};
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[2];
    // DSV用のヒープでディスクリプタの数は１。DSVはshader内で触るものではないので,ShaderVisibleはfalse
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap = nullptr;



    uint32_t descriptorSizeRTV = 0;
    uint32_t descriptorSizeDSV = 0;
    // スワップチェーンから取得したバックバッファリソース（2枚分）
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, 2> swapChainResources;
    // フェンス
    Microsoft::WRL::ComPtr<ID3D12Fence> fence = nullptr;
    // フェンス値
    uint64_t fenceValue = 0;
    // フェンスのSignalを待つためのイベント
    HANDLE fenceEvent = nullptr;
    //  WindowsAPI
    WinApp* winApp_ = nullptr;
    // ビューポート
    D3D12_VIEWPORT viewport {};
    // シザー矩形
    D3D12_RECT scissorRect {};
    // DXCユーティリティ
    Microsoft::WRL::ComPtr<IDxcUtils> dxcUtils;
    // DXCコンパイラ
    Microsoft::WRL::ComPtr<IDxcCompiler3> dxcCompiler;
    // DXCインクルードハンドラ
    Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler;
    // デバイス初期化関数
    void InitializeDevice();
    // コマンド初期化
    void InitializeCommand();
    // スワップチェイン
    void InitializeSwapChain();
    // 深度バッファ
    void InitializeDepthBuffer();
    // ディスクリプタヒープ
    void InitializeDescriptorHeaps();
    // RTVの初期化
    void InitializeRenderTargetView();
    // DSVの初期化
    void InitializeDepthStencilView();
    // フェンスの初期化
    void InitializeFence();
    // ビューポート矩形の初期化
    void InitializeViewport();
    // シザー矩形の初期化
    void InitializeScissorRect();
    // DXCコンパイラの生成
    void InitializeDxcCompiler();
    //// IMGUI初期化
    //void InitializeImGui();

};
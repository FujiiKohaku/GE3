#include "DirectXCommon.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx12.h"
#include "imgui/imgui_impl_win32.h"
#include "Engine/Winapp/WinApp.h"
#include <d3d12.h>
#include <dxgi1_6.h>
#include <filesystem>
#include <fstream>
#include <format>
#include <stdexcept>
#include <wrl.h>

namespace {
std::filesystem::path MakeCompiledShaderPath(const std::filesystem::path& hlslPath)
{
    const std::filesystem::path resourcesRoot = "resources";
    std::filesystem::path relativePath = hlslPath.lexically_relative(resourcesRoot);

    if (relativePath.empty() || relativePath.native().starts_with(L"..")) {
        return {};
    }

    std::filesystem::path compiledPath = resourcesRoot / "CompiledShaders" / relativePath;
    compiledPath.replace_extension(".dxil");
    return compiledPath;
}

std::wstring GetShaderProfile(const std::filesystem::path& sourcePath)
{
    const std::wstring fileName = sourcePath.filename().wstring();
    if (fileName.ends_with(L".VS.hlsl")) {
        return L"vs_6_0";
    }
    if (fileName.ends_with(L".PS.hlsl")) {
        return L"ps_6_0";
    }
    if (fileName.ends_with(L".CS.hlsl")) {
        return L"cs_6_0";
    }
    if (fileName.ends_with(L".GS.hlsl")) {
        return L"gs_6_0";
    }
    return {};
}

bool IsShaderCacheOutdated(
    const std::filesystem::path& sourcePath,
    const std::filesystem::path& compiledPath)
{
    if (!std::filesystem::is_regular_file(compiledPath)) {
        return true;
    }

    const std::filesystem::file_time_type cacheWriteTime =
        std::filesystem::last_write_time(compiledPath);
    if (std::filesystem::last_write_time(sourcePath) > cacheWriteTime) {
        return true;
    }

    const std::filesystem::path includeRoots[] = {
        std::filesystem::path("resources/Shaders"),
        std::filesystem::path("resources/Effects"),
    };

    for (const std::filesystem::path& includeRoot : includeRoots) {
        if (!std::filesystem::is_directory(includeRoot)) {
            continue;
        }

        for (const std::filesystem::directory_entry& entry :
            std::filesystem::recursive_directory_iterator(includeRoot)) {
            if (!entry.is_regular_file()) {
                continue;
            }
            if (entry.path().extension() != L".hlsli") {
                continue;
            }
            if (entry.last_write_time() > cacheWriteTime) {
                return true;
            }
        }
    }

    return false;
}
}

std::unique_ptr<DirectXCommon> DirectXCommon::instance_ = nullptr;
// Singleton Instance
DirectXCommon* DirectXCommon::GetInstance()
{
    if (!instance_) {
        instance_ = std::make_unique<DirectXCommon>(ConstructorKey {});
    }
    return instance_.get();
}
DirectXCommon::DirectXCommon(ConstructorKey)
{
}
void DirectXCommon::Finalize()
{
    instance_.reset();
}

void DirectXCommon::Initialize(WinApp* winApp)
{

    // NULLチェック
    assert(winApp);
    winApp_ = winApp;
    // ここに初期化処理を書いていく
    // デバイス初期化
    InitializeDevice();
    // コマンド初期化
    InitializeCommand();
    // スワップチェーン初期化
    InitializeSwapChain();
    // 深度バッファ初期化
    InitializeDepthBuffer();
    // ディスクリプタヒープ初期化
    InitializeDescriptorHeaps();
    // RTVの初期化
    InitializeRenderTargetView();
    // DSVの初期化
    InitializeDepthStencilView();
    // フェンスの初期化
    InitializeFence();
    // ビューポート矩形初期化
    InitializeViewport();
    // シザーの初期化
    InitializeScissorRect();
    // DXCコンパイラの生成
    InitializeDxcCompiler();
    // IMGUI初期化
    /* InitializeImGui();*/
}
#pragma region
//// SRVの指定番号のCPUデスクリプタハンドルを取得する
// D3D12_CPU_DESCRIPTOR_HANDLE DirectXCommon::GetSRVCPUDescriptorHandle(uint32_t index)
//{
//     return GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, index);
// }
//// SRVの指定番号のGPUデスクリプタハンドルを取得する
// D3D12_GPU_DESCRIPTOR_HANDLE DirectXCommon::GetSRVGPUDescriptorHandle(uint32_t index)
//{
//     return GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, index);
// }
#pragma endregion
#pragma region

// デバイス初期化関数
void DirectXCommon::InitializeDevice()
{
    HRESULT hr;
    // デバッグレイヤーをONにする
#ifdef _DEBUG

    Microsoft::WRL::ComPtr<ID3D12Debug1> debugController = nullptr; // COM
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
        // デバックレイヤーを有効化する
        debugController->EnableDebugLayer();
        // さらに6PU側でもチェックリストを行うようにする
        debugController->SetEnableGPUBasedValidation(TRUE);
    }
#endif // _DEBUG

   

    #ifdef _DEBUG
    hr = CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(dxgiFactory.GetAddressOf()));
#else
    hr = CreateDXGIFactory2(0, IID_PPV_ARGS(dxgiFactory.GetAddressOf()));
#endif
    // 初期化の根本的な部分でエラーが出た場合はプログラムが間違っているか、どうにもできない場合が多いのでassertにしておく
    assert(SUCCEEDED(hr));

    Microsoft::WRL::ComPtr<IDXGIAdapter4> useAdapter = nullptr;

    // よい順にアダプタを頼む
    for (UINT i = 0; dxgiFactory->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(useAdapter.GetAddressOf())) != DXGI_ERROR_NOT_FOUND; ++i) {

        // アダプターの情報を取得する
        DXGI_ADAPTER_DESC3 adapterDesc {};
        hr = useAdapter->GetDesc3(&adapterDesc);
        assert(SUCCEEDED(hr)); // 取得できないのは一大事
        // ソフトウェアアダプタでなければ採用!
        if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) {
            // 採用したアダプタの情報をログに出力wstringの方なので注意
            Logger::Log(StringUtility::ConvertString(std::format(L"Use Adapater:{}\n", adapterDesc.Description)));
            break;
        }
        useAdapter = nullptr; // ソフトウェアアダプタの場合は見なかったことにする
    }

    // 適切なアダプタが見つからなかったので起動できない
    assert(useAdapter != nullptr);

    // 昨日レベルとログ出力用の文字列
    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_12_2, D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0
    };

    const char* featureLevelStrings[] = { "12.2", "12.1", "12.0" };
    // 高い順に生成できるか試していく
    for (size_t i = 0; i < _countof(featureLevels); ++i) {
        // 採用したアダプターでデバイスを生成
        hr = D3D12CreateDevice(useAdapter.Get(), featureLevels[i], IID_PPV_ARGS(&device));
        // 指定した昨日レベルでデバイスは生成できたか確認
        if (SUCCEEDED(hr)) {
            // 生成できたのでログ出力を行ってループを抜ける

            Logger::Log(std::format("FeatureLevel : {}\n", featureLevelStrings[i]));
            break;
        }
    }
    // デバイスの生成が上手くいかなかったので起動できない
    assert(device != nullptr);
    Logger::Log("Complete create D3D12Device!!!\n"); // 初期化完了のログを出す

#ifdef _DEBUG

    Microsoft::WRL::ComPtr<ID3D12InfoQueue>
        infoQueue = nullptr;
    if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
        // やばいエラー時に止まる
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
        // エラー時に止まる
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
        // 警告時に止まる//これ消すとデバッグできる
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
        // 抑制するメッセージのＩＤ
        D3D12_MESSAGE_ID denyIds[] = {
            // windows11でのDXGIデバックレイヤーとDX12デバックレイヤーの相互作用バグによるエラーメッセージ
            // https://stackoverflow.com/questions/69805245/directx-12-application-is-crashing-in-windows-11
            D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE
        };
        // 抑制するレベル
        D3D12_MESSAGE_SEVERITY severities[] = { D3D12_MESSAGE_SEVERITY_INFO };
        D3D12_INFO_QUEUE_FILTER filter {};
        filter.DenyList.NumIDs = _countof(denyIds);
        filter.DenyList.pIDList = denyIds;
        filter.DenyList.NumSeverities = _countof(severities);
        filter.DenyList.pSeverityList = severities;
        // 指定したメッセージの表示wp抑制する
        infoQueue->PushStorageFilter(&filter);
        // 解放
        /*  infoQueue->Release();*/
    }
#endif // DEBUG
}
#pragma endregion
#pragma region
void DirectXCommon::InitializeCommand()
{
    HRESULT hr;

    // コマンドキューを生成する
    D3D12_COMMAND_QUEUE_DESC commandQueueDesc {};
    hr = device->CreateCommandQueue(&commandQueueDesc,IID_PPV_ARGS(&commandQueue));
    // コマンドキューの生成が上手くいかなかったので起動できない
    assert(SUCCEEDED(hr));

    // コマンドアロケーターを生成する
    hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator));
    // コマンドキューアロケーターの生成があ上手くいかなかったので起動できない
    assert(SUCCEEDED(hr));

    // コマンドリストを生成する
    hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList));
    // コマンドリストの生成が上手くいかなかったので起動できない
    assert(SUCCEEDED(hr));
  
}
#pragma endregion
#pragma region
void DirectXCommon::InitializeSwapChain()
{
    HRESULT hr;

    // スワップチェーンを生成する

    swapChainDesc.Width = WinApp::kClientWidth; // 画面の幅。ウィンドウのクライアント領域を同じものんにしておく
    swapChainDesc.Height = WinApp::kClientHeight; // 画面の高さ。ウィンドウのクライアント領域を同じものにしておく
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // 色の形式
    swapChainDesc.SampleDesc.Count = 1; // マルチサンプルしない
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; // 描画のターゲットとしてりようする
    swapChainDesc.BufferCount = 2; // ダブルバッファ
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // モニターに移したら,中身を吐き
    // コマンドキュー,ウィンドウバンドル、設定を渡して生成する
    hr = dxgiFactory->CreateSwapChainForHwnd(commandQueue.Get(), winApp_->GetHwnd(), &swapChainDesc, nullptr, nullptr, reinterpret_cast<IDXGISwapChain1**>(swapChain.GetAddressOf())); // com.Get,OF
    assert(SUCCEEDED(hr));
}
#pragma endregion
#pragma region
void DirectXCommon::InitializeDepthBuffer()
{
    HRESULT hr;

    // === 生成するResourceの設定 ===
    D3D12_RESOURCE_DESC resourceDesc {};
    resourceDesc.Width = WinApp::kClientWidth;
    resourceDesc.Height = WinApp::kClientHeight;
    resourceDesc.MipLevels = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    // === 利用するHeapの設定 ===
    D3D12_HEAP_PROPERTIES heapProperties {};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

    // === 深度値のクリア設定 ===
    D3D12_CLEAR_VALUE depthClearValue {};
    depthClearValue.DepthStencil.Depth = 1.0f;
    depthClearValue.DepthStencil.Stencil = 0;
    depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;

    // === Resourceの生成 ===　
    hr = device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &depthClearValue,
        IID_PPV_ARGS(&depthStencilResource));

    assert(SUCCEEDED(hr));
}
#pragma endregion
#pragma region
// ディスクリプタヒープ生成関数
Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> DirectXCommon::CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisivle)
{
    // ディスクリプタヒープの生成02_02
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> DescriptorHeap = nullptr;

    D3D12_DESCRIPTOR_HEAP_DESC DescriptorHeapDesc {};
    DescriptorHeapDesc.Type = heapType;
    DescriptorHeapDesc.NumDescriptors = numDescriptors;
    DescriptorHeapDesc.Flags = shaderVisivle ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    HRESULT hr = device->CreateDescriptorHeap(&DescriptorHeapDesc, IID_PPV_ARGS(&DescriptorHeap));
    // ディスクリプタヒープが作れなかったので起動できない
    assert(SUCCEEDED(hr)); // 1
    return DescriptorHeap;
}
#pragma endregion
#pragma region

// 指定番号のCPUディスクリプタハンドルを取得する関数
D3D12_CPU_DESCRIPTOR_HANDLE DirectXCommon::GetCPUDescriptorHandle(const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap, uint32_t descriptorSize, uint32_t index)
{
    D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
    handleCPU.ptr += (descriptorSize * index);
    return handleCPU;
}
// 指定番号のGPUディスクリプタハンドルを取得する関数
D3D12_GPU_DESCRIPTOR_HANDLE DirectXCommon::GetGPUDescriptorHandle(const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap, uint32_t descriptorSize, uint32_t index)
{
    D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
    handleGPU.ptr += (descriptorSize * index);
    return handleGPU;
}
#pragma endregion
#pragma region
void DirectXCommon::InitializeDescriptorHeaps()
{

    /* descriptorSizeSRV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);*/
    descriptorSizeRTV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    descriptorSizeDSV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    // RTV用のヒープ（Shaderからは使わないのでfalse）
    rtvDescriptorHeap = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 8, false);

    // DSV用のヒープ（Shaderからは使わないのでfalse）
    dsvDescriptorHeap = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV,3,false);

    //// SRV用のヒープ（Shaderから使うのでtrue）
    // srvDescriptorHeap = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, kMaxSRVCount, true);
}
#pragma endregion
#pragma region
// RTVの初期化
void DirectXCommon::InitializeRenderTargetView()
{
    HRESULT hr;

    // スワップチェーンからリソースを取得（バックバッファ）
    for (uint32_t i = 0; i < swapChainResources.size(); ++i) {
        hr = swapChain->GetBuffer(i, IID_PPV_ARGS(&swapChainResources[i]));
        assert(SUCCEEDED(hr));
    }

    // RTVの設定
    rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

    // RTVハンドルの先頭を取得
    D3D12_CPU_DESCRIPTOR_HANDLE rtvStartHandle = rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

    // バックバッファ（2枚）分RTVを作成
    for (uint32_t i = 0; i < swapChainResources.size(); ++i) {
        // i番目のRTVハンドルを配列に保存
        rtvHandles[i] = rtvStartHandle;

        // RenderTargetViewの生成
        device->CreateRenderTargetView(swapChainResources[i].Get(), &rtvDesc, rtvHandles[i]);

        // 次のディスクリプタ位置に進める
        rtvStartHandle.ptr += descriptorSizeRTV;
    }
}
#pragma endregion
#pragma region
// DSVの初期化
void DirectXCommon::InitializeDepthStencilView()
{
    // DSVの設定
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc {};
    dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    // DSVHeapの先端にDSVを作る
    device->CreateDepthStencilView(depthStencilResource.Get(), &dsvDesc, dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
}
#pragma endregion
#pragma region
// フェンスの初期化
void DirectXCommon::InitializeFence()
{
    HRESULT hr;
    fenceValue = 0; // 0からスタートする
    hr = device->CreateFence(fenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
    assert(SUCCEEDED(hr));

    // FenceのSignalを待つためのイベントを作成する
    fenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    assert(fenceEvent != nullptr);
}
#pragma endregion
#pragma region
void DirectXCommon::InitializeViewport()
{
    // クライアント領域のサイズと一緒にして画面全体に表示する
    viewport.Width = WinApp::kClientWidth;
    viewport.Height = WinApp::kClientHeight;
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
}
#pragma endregion
#pragma region
void DirectXCommon::InitializeScissorRect()
{
    // 基本的にビューポートと同じ矩形が構成されるようにする
    scissorRect.left = 0;
    scissorRect.right = WinApp::kClientWidth;
    scissorRect.top = 0;
    scissorRect.bottom = WinApp::kClientHeight;
}
#pragma endregion
#pragma region
void DirectXCommon::InitializeDxcCompiler()
{
    HRESULT hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils));
    assert(SUCCEEDED(hr));

    hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler));
    assert(SUCCEEDED(hr));

    hr = dxcUtils->CreateDefaultIncludeHandler(&dxcIncludeHandler);
    assert(SUCCEEDED(hr));
}
#pragma endregion
#pragma region
// 描画前処理
void DirectXCommon::PreDraw()
{
    // これから書き込むバックバッファの番号を取得
    UINT backBufferIndex = swapChain->GetCurrentBackBufferIndex();
    // リソースバリアで書き込み可能に変更
    D3D12_RESOURCE_BARRIER barrier {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = swapChainResources[backBufferIndex].Get(); // バリアをはる対象のリソース。現在のバックバッファに対して行う
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT; // 遷移前(現在)のResourceState
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET; // 遷移後のResourceState
    commandList->ResourceBarrier(1, &barrier); // TransitionBarrierを張る


    // 描画先のRTVとDSVを設定する
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    commandList->OMSetRenderTargets(1, &rtvHandles[backBufferIndex], false, &dsvHandle);
    //  指定した色で画面全体をクリアする
    float clearColor[] = { 0.1f, 0.25f, 0.5f, 1.0f }; /// 青っぽい色RGBAの順これ最初の文字1.0fにするとピンク画面になる
    commandList->ClearRenderTargetView(rtvHandles[backBufferIndex], clearColor, 0, nullptr);
    // 画面全体の深度をクリア
    commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    // SRVのディスクリプタヒープをセットする

    /* ID3D12DescriptorHeap* descriptorHeaps[] = { srvDescriptorHeap.Get() };
       commandList->SetDescriptorHeaps(1, descriptorHeaps);*/
    // ビューポート領域の設定
    commandList->RSSetViewports(1, &viewport); // viewportを設定
    // シザー矩形の設定
    commandList->RSSetScissorRects(1, &scissorRect); // Scirssorを設定
}

// 描画後処理
void DirectXCommon::PostDraw()
 {

    // これから書き込むバックバッファの番号を取得
    UINT backBufferIndex = swapChain->GetCurrentBackBufferIndex();

    // リソースバリアでプレゼント可能に変更
    D3D12_RESOURCE_BARRIER barrier {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = swapChainResources[backBufferIndex].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    commandList->ResourceBarrier(1, &barrier);

    // グラフィックコマンドをクローズ
    HRESULT hr = commandList->Close();
    assert(SUCCEEDED(hr));

    // GPUコマンドの実行
    // GPUにコマンドリストの実行を行わせる;
    ID3D12CommandList* commandLists[] = { commandList.Get() };
    commandQueue->ExecuteCommandLists(_countof(commandLists), commandLists); // GPUに命令を送る
    // GPU画面の交換を通知
    swapChain->Present(1, 0);
    assert(SUCCEEDED(hr));
    // フェンスの値を更新
    fenceValue++;
    // コマンドキューにシグナルを送る
    commandQueue->Signal(fence.Get(), fenceValue);
    // コマンド完了待ち
    if (fence->GetCompletedValue() < fenceValue) {

        fence->SetEventOnCompletion(fenceValue, fenceEvent);
        WaitForSingleObject(fenceEvent, INFINITE);
    }
    // コマンドアロケータ―のリセット
    hr = commandAllocator->Reset();
    assert(SUCCEEDED(hr));
    // コマンドリストのリセット
    hr = commandList->Reset(commandAllocator.Get(), nullptr);
    assert(SUCCEEDED(hr));
}

void DirectXCommon::SetBackBufferRenderTarget(D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle)
{
    UINT backBufferIndex = swapChain->GetCurrentBackBufferIndex();

    commandList->OMSetRenderTargets(1, &rtvHandles[backBufferIndex], false, &dsvHandle);
    commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &scissorRect);
}
#pragma endregion
#pragma region
Microsoft::WRL::ComPtr<IDxcBlob> DirectXCommon::LoadCompiledShader(const std::wstring& hlslPath)
{
    const std::filesystem::path sourcePath(hlslPath);
    const std::filesystem::path compiledPath = MakeCompiledShaderPath(sourcePath);

    if (compiledPath.empty()) {
        const std::string message =
            "Could not convert HLSL path to DXIL path. HLSL:" +
            StringUtility::ConvertString(hlslPath);
        Logger::Error(message);
        throw std::runtime_error(message);
    }

    if (!std::filesystem::is_regular_file(sourcePath)) {
        const std::string message =
            "Shader source file is missing. HLSL:" +
            std::filesystem::absolute(sourcePath).generic_string();
        Logger::Error(message);
        throw std::runtime_error(message);
    }

    if (IsShaderCacheOutdated(sourcePath, compiledPath)) {
        return CompileShaderAndSaveCache(sourcePath, compiledPath);
    }

    Microsoft::WRL::ComPtr<IDxcBlobEncoding> shaderBinary;
    HRESULT hr = dxcUtils->LoadFile(compiledPath.c_str(), nullptr, &shaderBinary);
    if (FAILED(hr) || !shaderBinary) {
        const std::string message =
            "Failed to load compiled shader. HLSL:" + sourcePath.generic_string() +
            " DXIL:" + std::filesystem::absolute(compiledPath).generic_string() +
            " HRESULT:" + std::to_string(static_cast<unsigned long>(hr));
        Logger::Error(message);
        throw std::runtime_error(message);
    }

    Microsoft::WRL::ComPtr<IDxcBlob> shaderBlob;
    hr = shaderBinary.As(&shaderBlob);
    if (FAILED(hr) || !shaderBlob) {
        const std::string message =
            "Failed to access compiled shader data. DXIL:" +
            std::filesystem::absolute(compiledPath).generic_string();
        Logger::Error(message);
        throw std::runtime_error(message);
    }

    Logger::Log(
        "Loaded compiled shader. HLSL:" + sourcePath.generic_string() +
        " DXIL:" + compiledPath.generic_string());
    return shaderBlob;
}

Microsoft::WRL::ComPtr<IDxcBlob> DirectXCommon::CompileShaderAndSaveCache(
    const std::filesystem::path& sourcePath,
    const std::filesystem::path& compiledPath)
{
    const std::wstring shaderProfile = GetShaderProfile(sourcePath);
    if (shaderProfile.empty()) {
        const std::string message =
            "Could not determine shader profile from file name. HLSL:" +
            sourcePath.generic_string();
        Logger::Error(message);
        throw std::runtime_error(message);
    }

    Microsoft::WRL::ComPtr<IDxcBlobEncoding> sourceBlob;
    HRESULT hr = dxcUtils->LoadFile(sourcePath.c_str(), nullptr, &sourceBlob);
    if (FAILED(hr) || !sourceBlob) {
        const std::string message =
            "Failed to load shader source. HLSL:" + sourcePath.generic_string() +
            " HRESULT:" + std::to_string(static_cast<unsigned long>(hr));
        Logger::Error(message);
        throw std::runtime_error(message);
    }

    const std::wstring absoluteIncludeDirectory =
        std::filesystem::absolute(sourcePath).parent_path().wstring();
    const wchar_t* arguments[] = {
        L"-E",
        L"main",
        L"-T",
        shaderProfile.c_str(),
        L"-O3",
        L"-Zpr",
        L"-I",
        absoluteIncludeDirectory.c_str(),
    };

    DxcBuffer sourceBuffer {};
    sourceBuffer.Ptr = sourceBlob->GetBufferPointer();
    sourceBuffer.Size = sourceBlob->GetBufferSize();
    sourceBuffer.Encoding = DXC_CP_UTF8;

    Microsoft::WRL::ComPtr<IDxcResult> compileResult;
    hr = dxcCompiler->Compile(
        &sourceBuffer,
        arguments,
        _countof(arguments),
        dxcIncludeHandler.Get(),
        IID_PPV_ARGS(&compileResult));
    if (FAILED(hr) || !compileResult) {
        const std::string message =
            "DXC failed to start shader compilation. HLSL:" + sourcePath.generic_string() +
            " HRESULT:" + std::to_string(static_cast<unsigned long>(hr));
        Logger::Error(message);
        throw std::runtime_error(message);
    }

    Microsoft::WRL::ComPtr<IDxcBlobUtf8> compileErrors;
    compileResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&compileErrors), nullptr);

    HRESULT compileStatus = S_OK;
    hr = compileResult->GetStatus(&compileStatus);
    if (compileErrors && compileErrors->GetStringLength() > 0) {
        const std::string compilerMessage =
            "Shader compiler output. HLSL:" + sourcePath.generic_string() + "\n" +
            std::string(compileErrors->GetStringPointer(), compileErrors->GetStringLength());
        if (FAILED(compileStatus)) {
            Logger::Error(compilerMessage);
        } else {
            Logger::Warning(compilerMessage);
        }
    }

    if (FAILED(hr) || FAILED(compileStatus)) {
        const std::string message =
            "Shader compilation failed. HLSL:" + sourcePath.generic_string() +
            " HRESULT:" + std::to_string(static_cast<unsigned long>(compileStatus));
        Logger::Error(message);
        throw std::runtime_error(message);
    }

    Microsoft::WRL::ComPtr<IDxcBlob> shaderBlob;
    hr = compileResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
    if (FAILED(hr) || !shaderBlob) {
        const std::string message =
            "Compiled shader object was not produced. HLSL:" + sourcePath.generic_string();
        Logger::Error(message);
        throw std::runtime_error(message);
    }

    std::error_code directoryError;
    std::filesystem::create_directories(compiledPath.parent_path(), directoryError);
    if (directoryError) {
        Logger::Error(
            "Could not create shader cache directory. DXIL:" +
            std::filesystem::absolute(compiledPath).generic_string() +
            " Error:" + directoryError.message());
    } else {
        std::ofstream cacheFile(compiledPath, std::ios::binary | std::ios::trunc);
        if (cacheFile) {
            cacheFile.write(
                static_cast<const char*>(shaderBlob->GetBufferPointer()),
                static_cast<std::streamsize>(shaderBlob->GetBufferSize()));
            if (!cacheFile) {
                Logger::Error(
                    "Failed while writing shader cache. DXIL:" +
                    std::filesystem::absolute(compiledPath).generic_string());
                std::error_code removeError;
                std::filesystem::remove(compiledPath, removeError);
            }
        } else {
            Logger::Error(
                "Could not open shader cache for writing. DXIL:" +
                std::filesystem::absolute(compiledPath).generic_string());
        }
    }

    Logger::Log(
        "Compiled shader and updated cache. HLSL:" + sourcePath.generic_string() +
        " DXIL:" + compiledPath.generic_string() +
        " Profile:" + StringUtility::ConvertString(shaderProfile));
    return shaderBlob;
}

// バッファリソース生成関数
Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::CreateBufferResource(size_t sizeInBytes)
{
    // 頂点リソース用のヒープの設定02_03
    D3D12_HEAP_PROPERTIES uploadHeapProperties {};
    uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD; // Uploadheapを使う
    // 頂点リソースの設定02_03
    D3D12_RESOURCE_DESC bufferDesc {};
    // バッファリソース。テクスチャの場合はまた別の設定をする02_03
    bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferDesc.Width = sizeInBytes; // リソースのサイズ　02_03
    // バッファの場合はこれらは１にする決まり02_03
    bufferDesc.Height = 1;
    bufferDesc.DepthOrArraySize = 1;
    bufferDesc.MipLevels = 1;
    bufferDesc.SampleDesc.Count = 1;
    // バッファの場合はこれにする決まり02_03
    bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    // 実際に頂点リソースを作る02_03
    Microsoft::WRL::ComPtr<ID3D12Resource> bufferResource = nullptr;
    HRESULT hr = device->CreateCommittedResource(
        &uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&bufferResource));
    assert(SUCCEEDED(hr));

    return bufferResource;
}
#pragma endregion
#pragma region
// テクスチャリソース生成関数
Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::CreateTextureResource(Microsoft::WRL::ComPtr<ID3D12Device> device, const DirectX::TexMetadata& metadata)
{
    // 1.metadataをもとにResourceの設定
    D3D12_RESOURCE_DESC resourceDesc {};
    resourceDesc.Width = UINT(metadata.width); // Textureの幅
    resourceDesc.Height = UINT(metadata.height); // Textureの高さ
    resourceDesc.MipLevels = UINT16(metadata.mipLevels); // mipdmapの数
    resourceDesc.DepthOrArraySize = UINT16(metadata.arraySize); // 奥行き　or 配列Textureの配列数
    resourceDesc.Format = metadata.format; // TextureのFormat
    resourceDesc.SampleDesc.Count = 1; // サンプリングカウント。1固定
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION(metadata.dimension); // Textureの次元数　普段使っているのは二次元
    // 2.利用するHeapの設定。非常に特殊な運用。02_04exで一般的なケース版がある
    D3D12_HEAP_PROPERTIES heapProperties {};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT; // 細かい設定を行う//03_00EX

    // 3.Resourceを生成する
    Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
    HRESULT hr = device->CreateCommittedResource(
        &heapProperties, // Heapの固定
        D3D12_HEAP_FLAG_NONE, // Heapの特殊な設定。特になし
        &resourceDesc, // Resourceの設定
        D3D12_RESOURCE_STATE_COPY_DEST, // 初回のResourceState.Textureは基本読むだけ//03_00EX
        nullptr, // Clear最適地。使わないのでnullptr
        IID_PPV_ARGS(&resource)); // 作成するResourceポインタへのポインタ
    assert(SUCCEEDED(hr));
    return resource;
}
#pragma endregion
#pragma region
// テクスチャアップロード関数
[[nodiscard]]
Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::UploadTextureData(Microsoft::WRL::ComPtr<ID3D12Resource> texture, const DirectX::ScratchImage& mipImages)
{
    std::vector<D3D12_SUBRESOURCE_DATA> subresources;
    DirectX::PrepareUpload(device.Get(), mipImages.GetImages(), mipImages.GetImageCount(), mipImages.GetMetadata(), subresources);

    uint64_t intermediateSize = GetRequiredIntermediateSize(texture.Get(), 0, static_cast<UINT>(subresources.size()));

    Microsoft::WRL::ComPtr<ID3D12Resource> intermediate = CreateBufferResource(intermediateSize);

    UpdateSubresources(commandList.Get(), texture.Get(), intermediate.Get(), 0, 0, static_cast<UINT>(subresources.size()), subresources.data());

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = texture.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &barrier);

    return intermediate;
}
#pragma endregion

// #pragma region テクスチャ読み込み関数
//// テクスチャ読み込み関数
// DirectX::ScratchImage DirectXCommon::LoadTexture(const std::string& filePath)
//{
//     // テクスチャファイルを読んでプログラムで扱えるようにする
//     DirectX::ScratchImage image {};
//     std::wstring filePathW = StringUtility::ConvertString(filePath);
//     HRESULT hr = DirectX::LoadFromWICFile(filePathW.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
//     // std::wcout << L"LoadFromWICFile HR: " << std::hex << hr << std::endl;
//     assert(SUCCEEDED(hr));
//
//     // ミップマップの作成
//     DirectX::ScratchImage mipImages {};
//     hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::TEX_FILTER_SRGB, 0, mipImages);
//     assert(SUCCEEDED(hr));
//
//     // ミップマップ付きのデータを返す
//     return mipImages;
// }
// #pragma endregion

void DirectXCommon::WaitForGPU()
{
    // フェンス値を更新してシグナル送信
    fenceValue++;
    commandQueue->Signal(fence.Get(), fenceValue);

    // GPUが完了していないなら待機
    if (fence->GetCompletedValue() < fenceValue) {
        fence->SetEventOnCompletion(fenceValue, fenceEvent);
        WaitForSingleObject(fenceEvent, INFINITE);
    }
}
D3D12_CPU_DESCRIPTOR_HANDLE DirectXCommon::GetRTVHandle(uint32_t index) const
{
    D3D12_CPU_DESCRIPTOR_HANDLE handle = rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<SIZE_T>(descriptorSizeRTV) * index;
    return handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE DirectXCommon::GetDSVHandle() const
{
    return dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
}
D3D12_CPU_DESCRIPTOR_HANDLE DirectXCommon::GetDSVHandle(uint32_t index) const
{
    D3D12_CPU_DESCRIPTOR_HANDLE handle = dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

    handle.ptr += static_cast<SIZE_T>(descriptorSizeDSV) * index;

    return handle;
}

#include "DirectXCommon.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx12.h"
#include "imgui/imgui_impl_win32.h"
#include <WinApp.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <format>
#include <wrl.h>

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
    InitializeImGui();
}

// 最大SRV数(最大テクスチャ枚数)
const uint32_t DirectXCommon::kMaxSRVCount = 512;

#pragma region SRV特化関数
// SRVの指定番号のCPUデスクリプタハンドルを取得する
D3D12_CPU_DESCRIPTOR_HANDLE DirectXCommon::GetSRVCPUDescriptorHandle(uint32_t index)
{
    return GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, index);
}
// SRVの指定番号のGPUデスクリプタハンドルを取得する
D3D12_GPU_DESCRIPTOR_HANDLE DirectXCommon::GetSRVGPUDescriptorHandle(uint32_t index)
{
    return GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, index);
}
#pragma endregion

#pragma region デバイス初期化

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
    // DXGIファクトリーの生成
    hr = CreateDXGIFactory(IID_PPV_ARGS(dxgiFactory.GetAddressOf()));
    // 初期化の根本的な部分でエラーが出た場合はプログラムが間違っているか、どうにもできない場合が多いのでassertにしておく
    assert(SUCCEEDED(hr));
    IDXGIAdapter4* useAdapter = nullptr; // com
    // よい順にアダプタを頼む
    for (UINT i = 0; dxgiFactory->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&useAdapter)) != DXGI_ERROR_NOT_FOUND; ++i) {

        // アダプターの情報を取得する
        DXGI_ADAPTER_DESC3 adapterDesc {}; // com
        hr = useAdapter->GetDesc3(&adapterDesc); // comGet
        assert(SUCCEEDED(hr)); // 取得できないのは一大事
        // ソフトウェアアダプタでなければ採用!
        if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) { // get
            // 採用したアダプタの情報をログに出力wstringの方なので注意
            Logger::Log(StringUtility::ConvertString(std::format(L"Use Adapater:{}\n", adapterDesc.Description))); // get
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
        hr = D3D12CreateDevice(useAdapter, featureLevels[i], IID_PPV_ARGS(&device));
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
        // 警告時に止まる
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

#pragma region コマンド初期化
void DirectXCommon::InitializeCommand()
{
    HRESULT hr;

    // コマンドキューを生成する
    D3D12_COMMAND_QUEUE_DESC commandQueueDesc {};
    hr = device->CreateCommandQueue(&commandQueueDesc,
        IID_PPV_ARGS(&commandQueue));
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

#pragma region スワップチェーン初期化
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

#pragma region 深度バッファ初期化
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

#pragma region ディスクリプタヒープ生成関数
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

#pragma region ディスクリプタハンドル取得関数

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

#pragma region ディスクリプタヒープ初期化
void DirectXCommon::InitializeDescriptorHeaps()
{

    descriptorSizeSRV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    descriptorSizeRTV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    descriptorSizeDSV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    // RTV用のヒープ（Shaderからは使わないのでfalse）
    rtvDescriptorHeap = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2, false);

    // DSV用のヒープ（Shaderからは使わないのでfalse）
    dsvDescriptorHeap = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);

    // SRV用のヒープ（Shaderから使うのでtrue）
    srvDescriptorHeap = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, kMaxSRVCount, true);
}

#pragma endregion

#pragma region RTV初期化
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

#pragma region DSV初期化
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

#pragma region フェンス初期化
// フェンスの初期化
void DirectXCommon::InitializeFence()
{
    HRESULT hr;
    fenceValue = 0;
    hr = device->CreateFence(fenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
    assert(SUCCEEDED(hr));

    // FenceのSignalを待つためのイベントを作成する
    fenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    assert(fenceEvent != nullptr);
}
#pragma endregion

#pragma region ビューポート初期化
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

#pragma region シザー初期化
void DirectXCommon::InitializeScissorRect()
{
    // 基本的にビューポートと同じ矩形が構成されるようにする
    scissorRect.left = 0;
    scissorRect.right = WinApp::kClientWidth;
    scissorRect.top = 0;
    scissorRect.bottom = WinApp::kClientHeight;
}
#pragma endregion

#pragma region DXCコンパイラ初期化
void DirectXCommon::InitializeDxcCompiler()
{
    HRESULT hr;

    hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils));
    assert(SUCCEEDED(hr));
    hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler));
    assert(SUCCEEDED(hr));

    // 現時点でincludeはしないがincludeに対応するための設定を行っておく
    hr = dxcUtils->CreateDefaultIncludeHandler(&includeHandler);
    assert(SUCCEEDED(hr));
}
#pragma endregion

#pragma region IMGUI初期化
void DirectXCommon::InitializeImGui()
{
    // バージョンチェック
    IMGUI_CHECKVERSION();
    // ImGuiのコンテキスト生成
    ImGui::CreateContext();
    // ImGuiのスタイル設定（好みで変更してよい）
    ImGui::StyleColorsClassic();
    // Win32用の初期化
    ImGui_ImplWin32_Init(winApp_->GetHwnd());
    // Direct12用の初期化
    ImGui_ImplDX12_Init(device.Get(), swapChainDesc.BufferCount, rtvDesc.Format,
        srvDescriptorHeap.Get(),
        srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
        srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
}
#pragma endregion

#pragma region 描画前処理・描画後処理
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

    // 描画先のRTVとDSVを設定する
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    commandList->OMSetRenderTargets(1, &rtvHandles[backBufferIndex], false, &dsvHandle);
    //  指定した色で画面全体をクリアする
    float clearColor[] = { 0.1f, 0.25f, 0.5f, 1.0f }; /// 青っぽい色RGBAの順これ最初の文字1.0fにするとピンク画面になる
    commandList->ClearRenderTargetView(rtvHandles[backBufferIndex], clearColor, 0, nullptr);
    // 画面全体の深度をクリア
    commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    // SRVのディスクリプタヒープをセットする

    ID3D12DescriptorHeap* descriptorHeaps[] = { srvDescriptorHeap.Get() };
    commandList->SetDescriptorHeaps(1, descriptorHeaps);
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
    commandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);
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
#pragma endregion

#pragma region シェーダーコンパイル関数
Microsoft::WRL::ComPtr<IDxcBlob> DirectXCommon::CompileShader(const std::wstring& filepath, const wchar_t* profile)
{
    // 1.hlslファイルを読み込む02_00
    Logger::Log(StringUtility::ConvertString(std::format(L"Begin CompileShader,path:{},profike:{}\n", filepath, profile))); // これからシェーダーをコンパイルする旨をログに出す
    // hlslファイルを読む
    Microsoft::WRL::ComPtr<IDxcBlobEncoding> shaderSource = nullptr;
    HRESULT hr = dxcUtils->LoadFile(filepath.c_str(), nullptr, &shaderSource);
    assert(SUCCEEDED(hr)); // 読めなかったら止める
    // 読み込んだファイルの内容を設定する
    DxcBuffer shaderSourceBuffer;
    shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
    shaderSourceBuffer.Size = shaderSource->GetBufferSize();
    shaderSourceBuffer.Encoding = DXC_CP_UTF8; // UTF8の文字コードであることを通知
    // 2.Compileする
    LPCWSTR arguments[] = {
        filepath.c_str(), // コンパイル対象のhlslファイル名
        L"-E",
        L"main", // エントリーポイントの指定。基本的にmain以外にはしない
        L"-T",
        profile, // shaderProfileの設定
        L"-Zi",
        L"-Qembed_debug", // デバック用の設定を埋め込む
        L"-Od", /// 最適化を外しておく
        L"-Zpr" // メモリレイアウトは行優先

    };
    // 実際にShaderをコンパイルする
    Microsoft::WRL::ComPtr<IDxcResult> shaderResult = nullptr;
    hr = dxcCompiler->Compile(

        &shaderSourceBuffer, // 読み込んだファイル
        arguments, // コンパイルオプション02_00
        _countof(arguments), // コンパイルオプションの数02_00
        includeHandler.Get(), // includeが含まれた諸々02_00
        IID_PPV_ARGS(&shaderResult) // コンパイル結果02_00
    );
    // コンパイルエラーではなくdxcが起動できないなど致命的な状況02_00
    assert(SUCCEEDED(hr));
    // 3.警告、エラーが出ていないか確認する02_00
    // 警告.エラーが出ていたらログに出して止める02_00
    IDxcBlobUtf8* shaderError = nullptr;
    shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);
    if (shaderError != nullptr && shaderError->GetStringLength() != 0) {
        Logger::Log(shaderError->GetStringPointer());
        // 警告、エラーダメ絶対02_00
        assert(false);
    }
    // 4.Compile結果を受け取って返す02_00
    // コンパイル結果から実行用のバイナリ部分を取得02_00
    Microsoft::WRL::ComPtr<IDxcBlob> shaderBlob = nullptr;
    hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob),
        nullptr);
    assert(SUCCEEDED(hr));
    // 成功したログを出す02_00
    Logger::Log(StringUtility::ConvertString(std::format(L"Compile Succeeded, path:{}, profike:{}\n ",
        filepath, profile)));

    // 実行用のバイナリを返却02_00
    return shaderBlob;
}

#pragma endregion

#pragma region バッファリソース生成関数
// バッファリソース生成関数
Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::CreateBufferResource(size_t sizeInBytes)
{
    // 頂点リソース用のヒープの設定02_03
    D3D12_HEAP_PROPERTIES uploadHeapProperties {};
    uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD; // Uploadheapを使う
    // 頂点リソースの設定02_03
    D3D12_RESOURCE_DESC vertexResourceDesc {};
    // バッファリソース。テクスチャの場合はまた別の設定をする02_03
    vertexResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    vertexResourceDesc.Width = sizeInBytes; // リソースのサイズ　02_03
    // バッファの場合はこれらは１にする決まり02_03
    vertexResourceDesc.Height = 1;
    vertexResourceDesc.DepthOrArraySize = 1;
    vertexResourceDesc.MipLevels = 1;
    vertexResourceDesc.SampleDesc.Count = 1;
    // バッファの場合はこれにする決まり02_03
    vertexResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    // 実際に頂点リソースを作る02_03
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource = nullptr;
    HRESULT hr = device->CreateCommittedResource(
        &uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &vertexResourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&vertexResource));
    assert(SUCCEEDED(hr));

    return vertexResource;
}
#pragma endregion

#pragma region テクスチャリソース生成関数
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

    // 消していいらしい
    //  heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK; //
    //  WriteBaackポリシーでCPUアクセス可能
    //  heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_L0; // プロセッサの近くに配置

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

#pragma region テクスチャアップロード関数
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
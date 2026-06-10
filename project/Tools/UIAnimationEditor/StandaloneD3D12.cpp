#include "StandaloneD3D12.h"
#include "d3dx12.h"
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include <cassert>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

UIEditorD3D12::UIEditorD3D12()
{
}

UIEditorD3D12::~UIEditorD3D12()
{
    Shutdown();
}

bool UIEditorD3D12::Initialize(HWND hwnd, int width, int height)
{
    hwnd_ = hwnd;
    width_ = width;
    height_ = height;

    UINT factoryFlags = 0;

#ifdef _DEBUG
    Microsoft::WRL::ComPtr<ID3D12Debug1> debugController;

    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
        debugController->EnableDebugLayer();
    }

    factoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

    HRESULT result = CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&dxgiFactory_));

    if (FAILED(result)) {
        return false;
    }

    Microsoft::WRL::ComPtr<IDXGIAdapter4> adapter;

    for (UINT adapterIndex = 0;
         dxgiFactory_->EnumAdapterByGpuPreference(adapterIndex, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter)) != DXGI_ERROR_NOT_FOUND;
         ++adapterIndex) {
        DXGI_ADAPTER_DESC3 adapterDescription = {};
        adapter->GetDesc3(&adapterDescription);

        if ((adapterDescription.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0) {
            break;
        }

        adapter.Reset();
    }

    if (adapter == nullptr) {
        return false;
    }

    result = D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device_));

    if (FAILED(result)) {
        return false;
    }

    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    result = device_->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue_));

    if (FAILED(result)) {
        return false;
    }

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = kFrameCount;
    swapChainDesc.Width = static_cast<UINT>(width_);
    swapChainDesc.Height = static_cast<UINT>(height_);
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;

    Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain;
    result = dxgiFactory_->CreateSwapChainForHwnd(commandQueue_.Get(), hwnd_, &swapChainDesc, nullptr, nullptr, &swapChain);

    if (FAILED(result)) {
        return false;
    }

    swapChain.As(&swapChain_);
    currentBackBufferIndex_ = static_cast<int>(swapChain_->GetCurrentBackBufferIndex());

    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.NumDescriptors = kFrameCount;

    result = device_->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap_));

    if (FAILED(result)) {
        return false;
    }

    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.NumDescriptors = kSrvDescriptorCount;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    result = device_->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&srvHeap_));

    if (FAILED(result)) {
        return false;
    }

    rtvDescriptorSize_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    srvDescriptorSize_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    for (int frameIndex = 0; frameIndex < kFrameCount; ++frameIndex) {
        result = swapChain_->GetBuffer(static_cast<UINT>(frameIndex), IID_PPV_ARGS(&backBuffers_[frameIndex]));

        if (FAILED(result)) {
            return false;
        }

        device_->CreateRenderTargetView(backBuffers_[frameIndex].Get(), nullptr, GetRtvHandle(frameIndex));

        result = device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocators_[frameIndex]));

        if (FAILED(result)) {
            return false;
        }
    }

    result = device_->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        commandAllocators_[0].Get(),
        nullptr,
        IID_PPV_ARGS(&commandList_));

    if (FAILED(result)) {
        return false;
    }

    commandList_->Close();

    result = device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&uploadCommandAllocator_));

    if (FAILED(result)) {
        return false;
    }

    result = device_->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        uploadCommandAllocator_.Get(),
        nullptr,
        IID_PPV_ARGS(&uploadCommandList_));

    if (FAILED(result)) {
        return false;
    }

    uploadCommandList_->Close();

    result = device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_));

    if (FAILED(result)) {
        return false;
    }

    fenceEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    if (fenceEvent_ == nullptr) {
        return false;
    }

    return true;
}

void UIEditorD3D12::Shutdown()
{
    if (device_ != nullptr) {
        WaitForGpu();
    }

    if (fenceEvent_ != nullptr) {
        CloseHandle(fenceEvent_);
        fenceEvent_ = nullptr;
    }

    for (int frameIndex = 0; frameIndex < kFrameCount; ++frameIndex) {
        backBuffers_[frameIndex].Reset();
        commandAllocators_[frameIndex].Reset();
    }

    uploadCommandList_.Reset();
    uploadCommandAllocator_.Reset();
    commandList_.Reset();
    fence_.Reset();
    srvHeap_.Reset();
    rtvHeap_.Reset();
    swapChain_.Reset();
    commandQueue_.Reset();
    device_.Reset();
    dxgiFactory_.Reset();
}

void UIEditorD3D12::Resize(int width, int height)
{
    if (swapChain_ == nullptr) {
        return;
    }
    if (device_ == nullptr) {
        return;
    }
    if (width <= 0 || height <= 0) {
        return;
    }
    if (width_ == width && height_ == height) {
        return;
    }

    WaitForGpu();

    for (int frameIndex = 0; frameIndex < kFrameCount; ++frameIndex) {
        backBuffers_[frameIndex].Reset();
    }

    HRESULT result = swapChain_->ResizeBuffers(
        kFrameCount,
        static_cast<UINT>(width),
        static_cast<UINT>(height),
        DXGI_FORMAT_R8G8B8A8_UNORM,
        0);

    if (FAILED(result)) {
        return;
    }

    width_ = width;
    height_ = height;
    currentBackBufferIndex_ = static_cast<int>(swapChain_->GetCurrentBackBufferIndex());

    for (int frameIndex = 0; frameIndex < kFrameCount; ++frameIndex) {
        result = swapChain_->GetBuffer(static_cast<UINT>(frameIndex), IID_PPV_ARGS(&backBuffers_[frameIndex]));

        if (FAILED(result)) {
            return;
        }

        device_->CreateRenderTargetView(backBuffers_[frameIndex].Get(), nullptr, GetRtvHandle(frameIndex));
    }
}

void UIEditorD3D12::BeginFrame()
{
    currentBackBufferIndex_ = static_cast<int>(swapChain_->GetCurrentBackBufferIndex());
    commandAllocators_[currentBackBufferIndex_]->Reset();
    commandList_->Reset(commandAllocators_[currentBackBufferIndex_].Get(), nullptr);

    D3D12_VIEWPORT viewport = {};
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = static_cast<float>(width_);
    viewport.Height = static_cast<float>(height_);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    D3D12_RECT scissorRect = {};
    scissorRect.left = 0;
    scissorRect.top = 0;
    scissorRect.right = static_cast<LONG>(width_);
    scissorRect.bottom = static_cast<LONG>(height_);

    commandList_->RSSetViewports(1, &viewport);
    commandList_->RSSetScissorRects(1, &scissorRect);

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = backBuffers_[currentBackBufferIndex_].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList_->ResourceBarrier(1, &barrier);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = GetRtvHandle(currentBackBufferIndex_);
    commandList_->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    float clearColor[4] = { 0.09f, 0.10f, 0.12f, 1.0f };
    commandList_->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
}

void UIEditorD3D12::EndFrame()
{
    ID3D12DescriptorHeap* descriptorHeaps[] = { srvHeap_.Get() };
    commandList_->SetDescriptorHeaps(1, descriptorHeaps);
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList_.Get());

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = backBuffers_[currentBackBufferIndex_].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList_->ResourceBarrier(1, &barrier);

    commandList_->Close();

    ID3D12CommandList* commandLists[] = { commandList_.Get() };
    commandQueue_->ExecuteCommandLists(1, commandLists);
    swapChain_->Present(1, 0);

    WaitForGpu();
}

void UIEditorD3D12::WaitForGpu()
{
    if (commandQueue_ == nullptr || fence_ == nullptr || fenceEvent_ == nullptr) {
        return;
    }

    fenceValue_++;
    commandQueue_->Signal(fence_.Get(), fenceValue_);

    if (fence_->GetCompletedValue() < fenceValue_) {
        fence_->SetEventOnCompletion(fenceValue_, fenceEvent_);
        WaitForSingleObject(fenceEvent_, INFINITE);
    }
}

ID3D12Device* UIEditorD3D12::GetDevice() const
{
    return device_.Get();
}

ID3D12GraphicsCommandList* UIEditorD3D12::GetCommandList() const
{
    return commandList_.Get();
}

ID3D12DescriptorHeap* UIEditorD3D12::GetSrvDescriptorHeap() const
{
    return srvHeap_.Get();
}

int UIEditorD3D12::GetWidth() const
{
    return width_;
}

int UIEditorD3D12::GetHeight() const
{
    return height_;
}

D3D12_CPU_DESCRIPTOR_HANDLE UIEditorD3D12::GetFontCpuHandle() const
{
    return GetSrvCpuHandle(0);
}

D3D12_GPU_DESCRIPTOR_HANDLE UIEditorD3D12::GetFontGpuHandle() const
{
    return GetSrvGpuHandle(0);
}

bool UIEditorD3D12::CreateTextureFromRGBA(
    const unsigned char* pixels,
    int width,
    int height,
    UIEditorTextureResource* textureResource)
{
    if (pixels == nullptr) {
        return false;
    }
    if (textureResource == nullptr) {
        return false;
    }
    if (width <= 0 || height <= 0) {
        return false;
    }

    D3D12_RESOURCE_DESC textureDesc = {};
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDesc.Alignment = 0;
    textureDesc.Width = static_cast<UINT64>(width);
    textureDesc.Height = static_cast<UINT>(height);
    textureDesc.DepthOrArraySize = 1;
    textureDesc.MipLevels = 1;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    CD3DX12_HEAP_PROPERTIES defaultHeapProperties(D3D12_HEAP_TYPE_DEFAULT);
    HRESULT result = device_->CreateCommittedResource(
        &defaultHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &textureDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&textureResource->texture));

    if (FAILED(result)) {
        return false;
    }

    UINT64 uploadBufferSize = GetRequiredIntermediateSize(textureResource->texture.Get(), 0, 1);
    CD3DX12_HEAP_PROPERTIES uploadHeapProperties(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC uploadBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);

    result = device_->CreateCommittedResource(
        &uploadHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &uploadBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&textureResource->uploadBuffer));

    if (FAILED(result)) {
        return false;
    }

    D3D12_SUBRESOURCE_DATA subresourceData = {};
    subresourceData.pData = pixels;
    subresourceData.RowPitch = static_cast<LONG_PTR>(width * 4);
    subresourceData.SlicePitch = static_cast<LONG_PTR>(width * height * 4);

    uploadCommandAllocator_->Reset();
    uploadCommandList_->Reset(uploadCommandAllocator_.Get(), nullptr);

    UpdateSubresources(uploadCommandList_.Get(), textureResource->texture.Get(), textureResource->uploadBuffer.Get(), 0, 0, 1, &subresourceData);

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = textureResource->texture.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    uploadCommandList_->ResourceBarrier(1, &barrier);

    uploadCommandList_->Close();

    ID3D12CommandList* commandLists[] = { uploadCommandList_.Get() };
    commandQueue_->ExecuteCommandLists(1, commandLists);
    WaitForGpu();

    int srvIndex = AllocateSrvDescriptor();

    if (srvIndex < 0) {
        return false;
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels = 1;

    textureResource->cpuHandle = GetSrvCpuHandle(srvIndex);
    textureResource->gpuHandle = GetSrvGpuHandle(srvIndex);
    textureResource->width = width;
    textureResource->height = height;
    textureResource->isValid = true;
    device_->CreateShaderResourceView(textureResource->texture.Get(), &srvDesc, textureResource->cpuHandle);

    return true;
}

D3D12_CPU_DESCRIPTOR_HANDLE UIEditorD3D12::GetRtvHandle(int index) const
{
    D3D12_CPU_DESCRIPTOR_HANDLE handle = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<SIZE_T>(index) * static_cast<SIZE_T>(rtvDescriptorSize_);
    return handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE UIEditorD3D12::GetSrvCpuHandle(int index) const
{
    D3D12_CPU_DESCRIPTOR_HANDLE handle = srvHeap_->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<SIZE_T>(index) * static_cast<SIZE_T>(srvDescriptorSize_);
    return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE UIEditorD3D12::GetSrvGpuHandle(int index) const
{
    D3D12_GPU_DESCRIPTOR_HANDLE handle = srvHeap_->GetGPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<UINT64>(index) * static_cast<UINT64>(srvDescriptorSize_);
    return handle;
}

int UIEditorD3D12::AllocateSrvDescriptor()
{
    if (nextSrvIndex_ >= kSrvDescriptorCount) {
        return -1;
    }

    int descriptorIndex = nextSrvIndex_;
    nextSrvIndex_++;

    return descriptorIndex;
}

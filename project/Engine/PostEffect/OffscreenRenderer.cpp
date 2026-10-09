#include "OffscreenRenderer.h"

#include <cassert>
#include <stdexcept>

#include "Engine/DirectXCommon/DirectXCommon.h"

OffscreenRenderer::~OffscreenRenderer()
{
    if (localLightSrvIndex_ != UINT_MAX) { SrvManager::GetInstance()->Free(localLightSrvIndex_); }
    for (uint32_t index : reflectionSrvIndices_) { if (index != UINT_MAX) { SrvManager::GetInstance()->Free(index); } }
    if (directionalLightSrvIndex_ != UINT_MAX) { SrvManager::GetInstance()->Free(directionalLightSrvIndex_); }
    if (materialSrvIndex_ != kInvalidDescriptorIndex) {
        SrvManager::GetInstance()->Free(materialSrvIndex_);
    }
    if (indirectSrvIndex_ != kInvalidDescriptorIndex) {
        SrvManager::GetInstance()->Free(indirectSrvIndex_);
    }
    if (srvIndex_ != kInvalidDescriptorIndex) {
        SrvManager::GetInstance()->Free(srvIndex_);
        srvIndex_ = kInvalidDescriptorIndex;
    }
    if (normalSrvIndex_ != kInvalidDescriptorIndex) {
        SrvManager::GetInstance()->Free(normalSrvIndex_);
        normalSrvIndex_ = kInvalidDescriptorIndex;
    }
}

void OffscreenRenderer::Initialize()
{
    format_ = DXGI_FORMAT_R16G16B16A16_FLOAT;
    clearColor_ = { 0.4f, 0.7f, 1.0f, 1.0f };

    CreateRenderTexture();
    CreateDescriptorViews();

    viewport_.Width = static_cast<float>(WinApp::kClientWidth);
    viewport_.Height = static_cast<float>(WinApp::kClientHeight);
    viewport_.TopLeftX = 0.0f;
    viewport_.TopLeftY = 0.0f;
    viewport_.MinDepth = 0.0f;
    viewport_.MaxDepth = 1.0f;

    scissorRect_.left = 0;
    scissorRect_.top = 0;
    scissorRect_.right = WinApp::kClientWidth;
    scissorRect_.bottom = WinApp::kClientHeight;
}

void OffscreenRenderer::PreDraw(D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle, bool shouldCaptureDirectionalLight, bool shouldCaptureReflections, bool shouldCaptureLocalShadows)
{
    if (shouldCaptureLocalShadows) { shouldCaptureReflections = true; }
    DirectXCommon* directXCommon = DirectXCommon::GetInstance();
    ID3D12GraphicsCommandList* commandList = directXCommon->GetCommandList();
    isDirectionalCaptureActive_ = false;
    isReflectionCaptureActive_ = false;
    isLocalShadowCaptureActive_ = false;
    if (shouldCaptureDirectionalLight || shouldCaptureReflections) {
        try {
            if (!directionalLightTexture_) { CreateDirectionalLightTarget(); }
            isDirectionalCaptureActive_ = shouldCaptureDirectionalLight;
            auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(directionalLightTexture_.Get(),
                directionalLightState_, D3D12_RESOURCE_STATE_RENDER_TARGET);
            commandList->ResourceBarrier(1, &barrier);
            directionalLightState_ = D3D12_RESOURCE_STATE_RENDER_TARGET;
        } catch (const std::exception& error) { Logger::Error(error.what()); }
    }
    if (shouldCaptureReflections && directionalLightTexture_) {
        try {
            if (!reflectionRtvHeap_) { CreateReflectionTargets(); }
            for (const auto& texture : reflectionTextures_) {
                auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(texture.Get(),
                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
                commandList->ResourceBarrier(1, &barrier);
            }
            isReflectionRenderState_ = true; isReflectionCaptureActive_ = true;
        } catch (const std::exception& error) { Logger::Error(error.what()); }
    }
    if (shouldCaptureLocalShadows && isReflectionCaptureActive_) {
        try {
            if (!localLightTexture_) { CreateLocalLightTarget(); }
            auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(localLightTexture_.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
            commandList->ResourceBarrier(1, &barrier); isLocalShadowCaptureActive_ = true;
        } catch (const std::exception& error) { Logger::Error(error.what()); }
    }
    if (currentState_ != D3D12_RESOURCE_STATE_RENDER_TARGET) {
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = renderTextureResource_.Get();
        barrier.Transition.StateBefore = currentState_;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        commandList->ResourceBarrier(1, &barrier);
        currentState_ = D3D12_RESOURCE_STATE_RENDER_TARGET;
    }
    if (normalCurrentState_ != D3D12_RESOURCE_STATE_RENDER_TARGET) {
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = normalTextureResource_.Get();
        barrier.Transition.StateBefore = normalCurrentState_;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        commandList->ResourceBarrier(1, &barrier);
        normalCurrentState_ = D3D12_RESOURCE_STATE_RENDER_TARGET;
    }

    if (indirectCurrentState_ != D3D12_RESOURCE_STATE_RENDER_TARGET) {
        D3D12_RESOURCE_BARRIER barrier {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = indirectTextureResource_.Get();
        barrier.Transition.StateBefore = indirectCurrentState_;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        commandList->ResourceBarrier(1, &barrier);
        indirectCurrentState_ = D3D12_RESOURCE_STATE_RENDER_TARGET;
    }
    if (materialCurrentState_ != D3D12_RESOURCE_STATE_RENDER_TARGET) {
        D3D12_RESOURCE_BARRIER barrier {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = materialTextureResource_.Get();
        barrier.Transition.StateBefore = materialCurrentState_;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        commandList->ResourceBarrier(1, &barrier);
        materialCurrentState_ = D3D12_RESOURCE_STATE_RENDER_TARGET;
    }
    commandList->RSSetViewports(1, &viewport_);
    commandList->RSSetScissorRects(1, &scissorRect_);
    D3D12_CPU_DESCRIPTOR_HANDLE renderTargets[8] = {
        rtvHandle_, normalRtvHandle_, indirectRtvHandle_, materialRtvHandle_, {}, {}, {}, {}
    };
    uint32_t renderTargetCount = 4;
    if (isDirectionalCaptureActive_ || isReflectionCaptureActive_) {
        renderTargets[4] = directionalLightRtvHeap_->GetCPUDescriptorHandleForHeapStart();
        renderTargetCount = 5;
        const float kDirectionalClear[] = {0, 0, 0, -1};
        commandList->ClearRenderTargetView(renderTargets[4], kDirectionalClear, 0, nullptr);
    }
    if (isReflectionCaptureActive_) {
        renderTargetCount = 7;
        const float kSurfaceClear[] = {0, 0, 0, 1};
        const float kEnvironmentClear[] = {0, 0, 0, -1};
        uint32_t incrementBytes = directXCommon->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        renderTargets[5] = reflectionRtvHeap_->GetCPUDescriptorHandleForHeapStart();
        renderTargets[6] = renderTargets[5]; renderTargets[6].ptr += incrementBytes;
        commandList->ClearRenderTargetView(renderTargets[5], kSurfaceClear, 0, nullptr);
        commandList->ClearRenderTargetView(renderTargets[6], kEnvironmentClear, 0, nullptr);
    }
    if (isLocalShadowCaptureActive_) {
        renderTargetCount = 8; renderTargets[7] = localLightRtvHeap_->GetCPUDescriptorHandleForHeapStart();
        const float kLocalClear[] = {0, 0, 0, -1}; commandList->ClearRenderTargetView(renderTargets[7], kLocalClear, 0, nullptr);
    }
    commandList->OMSetRenderTargets(renderTargetCount, renderTargets, false, &dsvHandle);

    float clearColor[] = {
        clearColor_.x,
        clearColor_.y,
        clearColor_.z,
        clearColor_.w
    };
    commandList->ClearRenderTargetView(rtvHandle_, clearColor, 0, nullptr);
    const float kIndirectClearColor[] = { 0, 0, 0, 0 };
    commandList->ClearRenderTargetView(indirectRtvHandle_, kIndirectClearColor, 0, nullptr);
    commandList->ClearRenderTargetView(materialRtvHandle_, kIndirectClearColor, 0, nullptr);
    const float normalClearColor[] = { 0.5f, 0.5f, 1.0f, -1.0f };
    commandList->ClearRenderTargetView(normalRtvHandle_, normalClearColor, 0, nullptr);
}

void OffscreenRenderer::PostDraw()
{
    if (isLocalShadowCaptureActive_) {
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(localLightTexture_.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        DirectXCommon::GetInstance()->GetCommandList()->ResourceBarrier(1, &barrier);
    }
    if (isReflectionRenderState_) {
        for (const auto& texture : reflectionTextures_) {
            auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(texture.Get(),
                D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            DirectXCommon::GetInstance()->GetCommandList()->ResourceBarrier(1, &barrier);
        }
        isReflectionRenderState_ = false;
    }
    if (directionalLightState_ == D3D12_RESOURCE_STATE_RENDER_TARGET) {
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(directionalLightTexture_.Get(),
            directionalLightState_, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        DirectXCommon::GetInstance()->GetCommandList()->ResourceBarrier(1, &barrier);
        directionalLightState_ = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    }
    if (currentState_ == D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) {
        return;
    }

    ID3D12GraphicsCommandList* commandList = DirectXCommon::GetInstance()->GetCommandList();
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = renderTextureResource_.Get();
    barrier.Transition.StateBefore = currentState_;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &barrier);
    currentState_ = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    D3D12_RESOURCE_BARRIER indirectBarrier {};
    indirectBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    indirectBarrier.Transition.pResource = indirectTextureResource_.Get();
    indirectBarrier.Transition.StateBefore = indirectCurrentState_;
    indirectBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    indirectBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &indirectBarrier);
    indirectCurrentState_ = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    D3D12_RESOURCE_BARRIER materialBarrier {};
    materialBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    materialBarrier.Transition.pResource = materialTextureResource_.Get();
    materialBarrier.Transition.StateBefore = materialCurrentState_;
    materialBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    materialBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &materialBarrier);
    materialCurrentState_ = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

    if (normalCurrentState_ != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) {
        D3D12_RESOURCE_BARRIER normalBarrier = {};
        normalBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        normalBarrier.Transition.pResource = normalTextureResource_.Get();
        normalBarrier.Transition.StateBefore = normalCurrentState_;
        normalBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        normalBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        commandList->ResourceBarrier(1, &normalBarrier);
        normalCurrentState_ = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    }
}
void OffscreenRenderer::CreateLocalLightTarget() {
    auto* device = DirectXCommon::GetInstance()->GetDevice(); auto* srvManager = SrvManager::GetInstance();
    if (!srvManager->CanAllocate()) { throw std::runtime_error("RT local capture descriptors exhausted"); }
    auto heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    auto description = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R16G16B16A16_FLOAT, WinApp::kClientWidth, WinApp::kClientHeight,
        1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
    Microsoft::WRL::ComPtr<ID3D12Resource> texture;
    auto rtvHeap = DirectXCommon::GetInstance()->CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1, false);
    if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &description, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr, IID_PPV_ARGS(&texture)))) {
        throw std::runtime_error("RT local capture allocation failed");
    }
    localLightSrvIndex_ = srvManager->Allocate(); srvManager->CreateSRVforTexture2D(localLightSrvIndex_, texture.Get(), description.Format, 1);
    device->CreateRenderTargetView(texture.Get(), nullptr, rtvHeap->GetCPUDescriptorHandleForHeapStart());
    localLightTexture_ = texture; localLightRtvHeap_ = rtvHeap; localLightAllocationBytes_ = device->GetResourceAllocationInfo(0, 1, &description).SizeInBytes;
}
D3D12_GPU_DESCRIPTOR_HANDLE OffscreenRenderer::GetLocalLightSrv() const {
    if (!isLocalShadowCaptureActive_) { return {}; }
    return SrvManager::GetInstance()->GetGPUDescriptorHandle(localLightSrvIndex_);
}
void OffscreenRenderer::CreateReflectionTargets() {
    auto* device = DirectXCommon::GetInstance()->GetDevice();
    auto* srvManager = SrvManager::GetInstance();
    if (!srvManager->CanAllocate(2)) { throw std::runtime_error("Reflection capture descriptors exhausted"); }
    auto heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    const DXGI_FORMAT kFormats[] = {DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R32G32B32A32_FLOAT};
    auto rtvHeap = DirectXCommon::GetInstance()->CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2, false);
    uint32_t incrementBytes = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    for (uint32_t index = 0; index < 2; ++index) {
        auto description = CD3DX12_RESOURCE_DESC::Tex2D(kFormats[index], WinApp::kClientWidth, WinApp::kClientHeight,
            1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
        HRESULT result = device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &description,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr, IID_PPV_ARGS(&reflectionTextures_[index]));
        if (FAILED(result)) { throw std::runtime_error("Reflection capture allocation failed"); }
        reflectionAllocationBytes_ += device->GetResourceAllocationInfo(0, 1, &description).SizeInBytes;
        if (reflectionSrvIndices_[index] == UINT_MAX) { reflectionSrvIndices_[index] = srvManager->Allocate(); }
        srvManager->CreateSRVforTexture2D(reflectionSrvIndices_[index], reflectionTextures_[index].Get(), kFormats[index], 1);
        auto handle = rtvHeap->GetCPUDescriptorHandleForHeapStart(); handle.ptr += index * incrementBytes;
        device->CreateRenderTargetView(reflectionTextures_[index].Get(), nullptr, handle);
    }
    reflectionRtvHeap_ = rtvHeap;
}
D3D12_GPU_DESCRIPTOR_HANDLE OffscreenRenderer::GetReflectionSurfaceSrv() const {
    if (!isReflectionCaptureActive_) { return {}; }
    return SrvManager::GetInstance()->GetGPUDescriptorHandle(reflectionSrvIndices_[0]);
}
D3D12_GPU_DESCRIPTOR_HANDLE OffscreenRenderer::GetReflectionEnvironmentSrv() const {
    if (!isReflectionCaptureActive_) { return {}; }
    return SrvManager::GetInstance()->GetGPUDescriptorHandle(reflectionSrvIndices_[1]);
}

void OffscreenRenderer::CreateDirectionalLightTarget() {
    auto* device = DirectXCommon::GetInstance()->GetDevice();
    auto* srvManager = SrvManager::GetInstance();
    if (!srvManager->CanAllocate()) { throw std::runtime_error("RT directional capture descriptors exhausted"); }
    D3D12_DESCRIPTOR_HEAP_DESC heapDescription = {};
    heapDescription.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    heapDescription.NumDescriptors = 1;
    if (FAILED(device->CreateDescriptorHeap(&heapDescription, IID_PPV_ARGS(&directionalLightRtvHeap_)))) {
        throw std::runtime_error("RT directional capture RTV creation failed");
    }
    D3D12_HEAP_PROPERTIES heap = {};
    heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    auto description = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32G32B32A32_FLOAT,
        WinApp::kClientWidth, WinApp::kClientHeight, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
    if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &description,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr, IID_PPV_ARGS(&directionalLightTexture_)))) {
        throw std::runtime_error("RT directional capture texture creation failed");
    }
    device->CreateRenderTargetView(directionalLightTexture_.Get(), nullptr, directionalLightRtvHeap_->GetCPUDescriptorHandleForHeapStart());
    directionalLightAllocationBytes_ = device->GetResourceAllocationInfo(0, 1, &description).SizeInBytes;
    directionalLightSrvIndex_ = srvManager->Allocate();
    srvManager->CreateSRVforTexture2D(directionalLightSrvIndex_, directionalLightTexture_.Get(), description.Format, 1);
}
D3D12_GPU_DESCRIPTOR_HANDLE OffscreenRenderer::GetDirectionalLightSrv() const {
    if (!isDirectionalCaptureActive_ || directionalLightSrvIndex_ == UINT_MAX) { return {}; }
    return SrvManager::GetInstance()->GetGPUDescriptorHandle(directionalLightSrvIndex_);
}

Microsoft::WRL::ComPtr<ID3D12Resource> OffscreenRenderer::CreateRenderTextureResource(
    Microsoft::WRL::ComPtr<ID3D12Device> device,
    uint32_t width,
    uint32_t height,
    DXGI_FORMAT format,
    const Vector4& clearColor)
{
    Microsoft::WRL::ComPtr<ID3D12Resource> resource;

    D3D12_RESOURCE_DESC resourceDesc {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDesc.Width = width;
    resourceDesc.Height = height;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = format;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    D3D12_HEAP_PROPERTIES heapProperties {};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_CLEAR_VALUE clearValue {};
    clearValue.Format = format;
    clearValue.Color[0] = clearColor.x;
    clearValue.Color[1] = clearColor.y;
    clearValue.Color[2] = clearColor.z;
    clearValue.Color[3] = clearColor.w;

    HRESULT hr = device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        &clearValue,
        IID_PPV_ARGS(&resource));
    assert(SUCCEEDED(hr));
    return resource;
}

D3D12_GPU_DESCRIPTOR_HANDLE OffscreenRenderer::GetSrvHandleGPU() const
{
    return srvHandleGPU_;
}

D3D12_GPU_DESCRIPTOR_HANDLE OffscreenRenderer::GetNormalSrvHandleGPU() const
{
    return normalSrvHandleGPU_;
}

void OffscreenRenderer::CreateRenderTexture()
{
    renderTextureResource_ = CreateRenderTextureResource(
        DirectXCommon::GetInstance()->GetDevice(),
        WinApp::kClientWidth,
        WinApp::kClientHeight,
        format_,
        clearColor_);
    materialTextureResource_ = CreateRenderTextureResource(
        DirectXCommon::GetInstance()->GetDevice(), WinApp::kClientWidth, WinApp::kClientHeight,
        DXGI_FORMAT_R8G8B8A8_UNORM, { 0, 0, 0, 0 });
    indirectTextureResource_ = CreateRenderTextureResource(
        DirectXCommon::GetInstance()->GetDevice(), WinApp::kClientWidth, WinApp::kClientHeight,
        DXGI_FORMAT_R16G16B16A16_FLOAT, { 0, 0, 0, 0 });
    normalTextureResource_ = CreateRenderTextureResource(
        DirectXCommon::GetInstance()->GetDevice(),
        WinApp::kClientWidth,
        WinApp::kClientHeight,
        DXGI_FORMAT_R16G16B16A16_FLOAT,
        { 0.5f, 0.5f, 1.0f, -1.0f });
}

void OffscreenRenderer::CreateDescriptorViews()
{
    DirectXCommon* directXCommon = DirectXCommon::GetInstance();
    ID3D12Device* device = directXCommon->GetDevice();

    rtvHandle_ = directXCommon->GetRTVHandle(2);
    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.Format = format_;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    device->CreateRenderTargetView(renderTextureResource_.Get(), &rtvDesc, rtvHandle_);

    indirectRtvHandle_ = directXCommon->GetRTVHandle(9);
    device->CreateRenderTargetView(indirectTextureResource_.Get(), &rtvDesc, indirectRtvHandle_);
    materialRtvHandle_ = directXCommon->GetRTVHandle(10);
    D3D12_RENDER_TARGET_VIEW_DESC materialRtvDesc = rtvDesc;
    materialRtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    device->CreateRenderTargetView(materialTextureResource_.Get(), &materialRtvDesc, materialRtvHandle_);
    normalRtvHandle_ = directXCommon->GetRTVHandle(8);
    D3D12_RENDER_TARGET_VIEW_DESC normalRtvDesc = {};
    normalRtvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    normalRtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    device->CreateRenderTargetView(
        normalTextureResource_.Get(), &normalRtvDesc, normalRtvHandle_);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = format_;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    materialSrvIndex_ = SrvManager::GetInstance()->Allocate();
    materialSrvHandleGPU_ = SrvManager::GetInstance()->GetGPUDescriptorHandle(materialSrvIndex_);
    D3D12_SHADER_RESOURCE_VIEW_DESC materialSrvDesc = srvDesc;
    materialSrvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    device->CreateShaderResourceView(materialTextureResource_.Get(), &materialSrvDesc,
        SrvManager::GetInstance()->GetCPUDescriptorHandle(materialSrvIndex_));
    indirectSrvIndex_ = SrvManager::GetInstance()->Allocate();
    indirectSrvHandleGPU_ = SrvManager::GetInstance()->GetGPUDescriptorHandle(indirectSrvIndex_);
    device->CreateShaderResourceView(indirectTextureResource_.Get(), &srvDesc,
        SrvManager::GetInstance()->GetCPUDescriptorHandle(indirectSrvIndex_));
    srvIndex_ = SrvManager::GetInstance()->Allocate();
    srvHandleCPU_ = SrvManager::GetInstance()->GetCPUDescriptorHandle(srvIndex_);
    srvHandleGPU_ = SrvManager::GetInstance()->GetGPUDescriptorHandle(srvIndex_);
    device->CreateShaderResourceView(renderTextureResource_.Get(), &srvDesc, srvHandleCPU_);

    D3D12_SHADER_RESOURCE_VIEW_DESC normalSrvDesc = {};
    normalSrvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    normalSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    normalSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    normalSrvDesc.Texture2D.MipLevels = 1;
    normalSrvIndex_ = SrvManager::GetInstance()->Allocate();
    normalSrvHandleCPU_ = SrvManager::GetInstance()->GetCPUDescriptorHandle(normalSrvIndex_);
    normalSrvHandleGPU_ = SrvManager::GetInstance()->GetGPUDescriptorHandle(normalSrvIndex_);
    device->CreateShaderResourceView(
        normalTextureResource_.Get(), &normalSrvDesc, normalSrvHandleCPU_);
}

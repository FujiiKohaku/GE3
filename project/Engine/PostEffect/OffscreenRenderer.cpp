#include "OffscreenRenderer.h"

#include <cassert>

#include "Engine/DirectXCommon/DirectXCommon.h"
void OffscreenRenderer::Initialize()
{
    DirectXCommon* directXCommon = DirectXCommon::GetInstance();
    ID3D12Device* device = directXCommon->GetDevice();

    format_ = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    clearColor_ = { 1.0f, 0.0f, 0.0f, 1.0f };

    renderTextureResource_ = CreateRenderTextureResource(
        device,
        WinApp::kClientWidth,
        WinApp::kClientHeight,
        format_,
        clearColor_);

    rtvHandle_ = directXCommon->GetRTVHandle(2);

    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.Format = format_;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

    device->CreateRenderTargetView(renderTextureResource_.Get(), &rtvDesc, rtvHandle_);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = format_;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    srvIndex_ = SrvManager::GetInstance()->Allocate();
    srvHandleCPU_ = SrvManager::GetInstance()->GetCPUDescriptorHandle(srvIndex_);
    srvHandleGPU_ = SrvManager::GetInstance()->GetGPUDescriptorHandle(srvIndex_);

    device->CreateShaderResourceView(renderTextureResource_.Get(), &srvDesc, srvHandleCPU_);

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
void OffscreenRenderer::PreDraw()
{
    DirectXCommon* directXCommon = DirectXCommon::GetInstance();
    ID3D12GraphicsCommandList* commandList = directXCommon->GetCommandList();

    if (currentState_ != D3D12_RESOURCE_STATE_RENDER_TARGET) {
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = renderTextureResource_.Get();
        barrier.Transition.StateBefore = currentState_;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        commandList->ResourceBarrier(1, &barrier);

        currentState_ = D3D12_RESOURCE_STATE_RENDER_TARGET;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = directXCommon->GetDSVHandle();

    commandList->RSSetViewports(1, &viewport_);
    commandList->RSSetScissorRects(1, &scissorRect_);
    commandList->OMSetRenderTargets(1, &rtvHandle_, false, &dsvHandle);

    float clearColor[] = {
        clearColor_.x,
        clearColor_.y,
        clearColor_.z,
        clearColor_.w
    };

    commandList->ClearRenderTargetView(rtvHandle_, clearColor, 0, nullptr);
    commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
}

void OffscreenRenderer::PostDraw()
{
    DirectXCommon* directXCommon = DirectXCommon::GetInstance();
    ID3D12GraphicsCommandList* commandList = directXCommon->GetCommandList();

    if (currentState_ != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) {
        D3D12_RESOURCE_BARRIER barrier {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = renderTextureResource_.Get();
        barrier.Transition.StateBefore = currentState_;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        commandList->ResourceBarrier(1, &barrier);

        currentState_ = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    }
}

Microsoft::WRL::ComPtr<ID3D12Resource> OffscreenRenderer::CreateRenderTextureResource(Microsoft::WRL::ComPtr<ID3D12Device> device,
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

    HRESULT hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_RENDER_TARGET, &clearValue, IID_PPV_ARGS(&resource));

    assert(SUCCEEDED(hr));

    return resource;
}
D3D12_GPU_DESCRIPTOR_HANDLE OffscreenRenderer::GetSrvHandleGPU() const
{
    return srvHandleGPU_;
}
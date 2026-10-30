#include "PreviewTarget.h"
#include "Engine/SrvManager/SrvManager.h"
#include <wincodec.h>
#include <stdexcept>

PreviewTarget::~PreviewTarget()
{
    if (srvIndex_ != UINT_MAX) { SrvManager::GetInstance()->Free(srvIndex_); }
}

void PreviewTarget::Initialize(DirectXCommon* dx)
{
    dx_ = dx;
    const uint32_t width = WinApp::GetInstance()->GetRenderWidth();
    const uint32_t height = WinApp::GetInstance()->GetRenderHeight();
    D3D12_HEAP_PROPERTIES heap {};
    heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC desc {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = width; desc.Height = height; desc.DepthOrArraySize = 1; desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; desc.SampleDesc.Count = 1;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    D3D12_CLEAR_VALUE clear {};
    clear.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    Microsoft::WRL::ComPtr<ID3D12Resource> texture;
    HRESULT result = dx_->GetDevice()->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &clear, IID_PPV_ARGS(&texture));
    if (FAILED(result)) { throw std::runtime_error("プレビュー画像を確保できません"); }
    if (!rtvHeap_) { rtvHeap_ = dx_->CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1, false); }
    D3D12_RENDER_TARGET_VIEW_DESC rtv {};
    rtv.Format = clear.Format; rtv.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    auto* srv = SrvManager::GetInstance();
    if (srvIndex_ == UINT_MAX) {
        if (!srv->CanAllocate()) { throw std::runtime_error("プレビューの登録枠が不足しています"); }
        srvIndex_ = srv->Allocate();
    }
    texture_.Swap(texture); width_ = width; height_ = height;
    dx_->GetDevice()->CreateRenderTargetView(texture_.Get(), &rtv, GetRtv());
    D3D12_SHADER_RESOURCE_VIEW_DESC view {};
    view.Format = clear.Format; view.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    view.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; view.Texture2D.MipLevels = 1;
    dx_->GetDevice()->CreateShaderResourceView(texture_.Get(), &view, srv->GetCPUDescriptorHandle(srvIndex_));
}

void PreviewTarget::Resize()
{
    if (width_ == static_cast<uint32_t>(WinApp::GetInstance()->GetRenderWidth()) &&
        height_ == static_cast<uint32_t>(WinApp::GetInstance()->GetRenderHeight())) { return; }
    Initialize(dx_);
}

void PreviewTarget::Begin()
{
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(texture_.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    dx_->GetCommandList()->ResourceBarrier(1, &barrier);
}

void PreviewTarget::End()
{
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(texture_.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    dx_->GetCommandList()->ResourceBarrier(1, &barrier);
}

D3D12_CPU_DESCRIPTOR_HANDLE PreviewTarget::GetRtv() const { return rtvHeap_->GetCPUDescriptorHandleForHeapStart(); }
D3D12_GPU_DESCRIPTOR_HANDLE PreviewTarget::GetSrv() const { return SrvManager::GetInstance()->GetGPUDescriptorHandle(srvIndex_); }

void PreviewTarget::SavePng(const std::filesystem::path& path, ID3D12Resource* source) const
{
    if (!path.parent_path().empty()) { std::filesystem::create_directories(path.parent_path()); }
    D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    if (source != nullptr) { state = D3D12_RESOURCE_STATE_PRESENT; }
    else { source = texture_.Get(); }
    DirectX::ScratchImage image;
    HRESULT result = DirectX::CaptureTexture(dx_->GetCommandQueue(), source, false, image, state, state);
    if (FAILED(result)) { throw std::runtime_error("画像の取得に失敗しました"); }
    result = DirectX::SaveToWICFile(*image.GetImage(0, 0, 0), DirectX::WIC_FLAGS_NONE, GUID_ContainerFormatPng, path.c_str());
    if (FAILED(result)) { throw std::runtime_error("PNGの保存に失敗しました"); }
}

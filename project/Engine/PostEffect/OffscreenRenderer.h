#pragma once
#include "Engine/math/MathStruct.h"
#include <cstdint>
#include <d3d12.h>
#include <wrl.h>

#include "Engine/DirectXCommon/DirectXCommon.h"

#include "Engine/SrvManager/SrvManager.h"
class OffscreenRenderer {
public:
    void Initialize();
    void PreDraw();
    void PostDraw();

    D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandleGPU() const;

private:
    Microsoft::WRL::ComPtr<ID3D12Resource> resource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateRenderTextureResource(
        Microsoft::WRL::ComPtr<ID3D12Device> device,
        uint32_t width,
        uint32_t height,
        DXGI_FORMAT format,
        const Vector4& clearColor);
    
    Microsoft::WRL::ComPtr<ID3D12Resource> renderTextureResource_;
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle_;

    DXGI_FORMAT format_;
    Vector4 clearColor_;

    uint32_t srvIndex_ = 0;
    D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU_ {};

    D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU_ {};


    D3D12_RESOURCE_STATES currentState_ = D3D12_RESOURCE_STATE_RENDER_TARGET;

    D3D12_VIEWPORT viewport_ = {};
    D3D12_RECT scissorRect_ = {};
};

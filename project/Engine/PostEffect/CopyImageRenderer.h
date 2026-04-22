#pragma once
#include <d3d12.h>
#include <wrl.h>
#include "Engine/DirectXCommon/DirectXCommon.h"
class CopyImageRenderer {
public:
    void Initialize(DirectXCommon* dxCommon);
    void Draw(D3D12_GPU_DESCRIPTOR_HANDLE textureHandle);

private:
    void CreateRootSignature();
    void CreateGraphicsPipeline();

private:
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState_;
    DirectXCommon* dxCommon_ = nullptr;
};
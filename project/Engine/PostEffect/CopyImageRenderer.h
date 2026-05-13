#pragma once
#include "Engine/DirectXCommon/DirectXCommon.h"
#include <d3d12.h>
#include <unordered_map>
#include <wrl.h>
#include "PostEffectType.h"

class CopyImageRenderer {
public:
    void Initialize(DirectXCommon* dxCommon);
    void Draw(D3D12_GPU_DESCRIPTOR_HANDLE textureHandle, D3D12_GPU_DESCRIPTOR_HANDLE depthTextureHandle);

    void SetPostEffectType(PostEffectType postEffectType);

private:
    void CreateRootSignature();
  // void CreateGraphicsPipeline();
    Microsoft::WRL::ComPtr<ID3D12PipelineState> CreateGraphicsPipeline(const std::wstring& pixelShaderPath);

private:
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    std::unordered_map<PostEffectType, Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStates_;
    DirectXCommon* dxCommon_ = nullptr;
    PostEffectType currentPostEffectType_ = PostEffectType::Copy;
};
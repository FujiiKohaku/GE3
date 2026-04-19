#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include "Engine/blend/BlendUtil.h"
class DirectXCommon;

class ParticleRenderManager {
public:
    void Initialize(DirectXCommon* dxCommon);
    void PreDraw(int blendMode);

    ID3D12RootSignature* GetRootSignature() const { return rootSignature_.Get(); }



private:
    void CreateRootSignature();
    void CreateGraphicsPipeline();
   

private:
    DirectXCommon* dxCommon_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineStates_[kCountOfBlendMode];
};
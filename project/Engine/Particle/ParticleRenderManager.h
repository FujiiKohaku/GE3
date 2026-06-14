#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include "Engine/Renderer/Blend/BlendUtil.h"
class DirectXCommon;

//クラス説明
// このクラスは、パーティクルの描画に必要なルートシグネチャとパイプラインステートを管理するクラスです。パーティクルの描画前に適切なパイプラインステートを設定するための機能も提供します。
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
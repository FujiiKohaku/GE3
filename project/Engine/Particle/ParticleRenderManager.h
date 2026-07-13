#pragma once
#include <d3d12.h>
#include <dxcapi.h>
#include <dxgi1_6.h>
#include <string>
#include <unordered_map>
#include <wrl.h>
#include "Engine/blend/BlendUtil.h"
class DirectXCommon;

//クラス説明
// このクラスは、パーティクルの描画に必要なルートシグネチャとパイプラインステートを管理するクラスです。パーティクルの描画前に適切なパイプラインステートを設定するための機能も提供します。
class ParticleRenderManager {
public:
    struct GraphicsPipelineDesc {
        std::string effectName;
        std::string vertexShaderPath;
        std::string pixelShaderPath;
        BlendMode blendMode = kBlendModeAdd;
        bool depthTest = true;
        bool depthWrite = false;
        D3D12_CULL_MODE cullMode = D3D12_CULL_MODE_NONE;
    };

public:
    void Initialize(DirectXCommon* dxCommon);
    void PreDraw(int blendMode);
    void PreDraw();
    Microsoft::WRL::ComPtr<ID3D12PipelineState> CreateGraphicsPipeline(const GraphicsPipelineDesc& desc);

    ID3D12RootSignature* GetRootSignature() const { return rootSignature_.Get(); }



private:
    void CreateRootSignature();
    void CreateDefaultGraphicsPipelines();
    Microsoft::WRL::ComPtr<IDxcBlob> CompileShaderWithLog(
        const std::string& effectName,
        const std::string& shaderStage,
        const std::string& shaderPath,
        const wchar_t* profile);
    std::string MakePipelineCacheKey(const GraphicsPipelineDesc& desc) const;


private:
    DirectXCommon* dxCommon_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineStates_[kCountOfBlendMode];
    std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateCache_;
};

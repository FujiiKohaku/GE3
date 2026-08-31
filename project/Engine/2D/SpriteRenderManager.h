#pragma once

#include "Engine/blend/BlendUtil.h"
#include <chrono>
#include <d3d12.h>
#include <string>
#include <unordered_map>
#include <wrl.h>

class DirectXCommon;
struct SpriteFrameParameters;

struct SpriteGraphicsPipelineDesc {
    std::string vertexShaderPath = "resources/Shaders/Sprite/Standard/Render.VS.hlsl";
    std::string pixelShaderPath = "resources/Shaders/Sprite/Standard/Render.PS.hlsl";
    BlendMode blendMode = kBlendModeNormal;
    bool depthTest = false;
    bool depthWrite = false;
    D3D12_CULL_MODE cullMode = D3D12_CULL_MODE_NONE;
};

class SpriteRenderManager {
public:
    void Initialize(DirectXCommon* dxCommon);
    void PreDraw();
    void BindPipeline(const SpriteGraphicsPipelineDesc& desc);
    ~SpriteRenderManager();

private:
    void CreateRootSignature();
    Microsoft::WRL::ComPtr<ID3D12PipelineState> GetOrCreateGraphicsPipeline(
        const SpriteGraphicsPipelineDesc& desc);
    Microsoft::WRL::ComPtr<ID3D12PipelineState> CreateGraphicsPipeline(
        const SpriteGraphicsPipelineDesc& desc);
    std::string MakePipelineCacheKey(const SpriteGraphicsPipelineDesc& desc) const;
    std::string NormalizeShaderPath(const std::string& shaderPath) const;
    void CreateFrameParameterBuffer();
    void UpdateFrameParameters();

private:
    DirectXCommon* dxCommon_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateCache_;
    ID3D12PipelineState* currentPipelineState_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> frameParameterResource_;
    SpriteFrameParameters* frameParameterData_ = nullptr;
    std::chrono::steady_clock::time_point startTime_;
    std::chrono::steady_clock::time_point previousFrameTime_;
};

#pragma once

#include "Engine/DirectXCommon/DirectXCommon.h"
#include <array>
#include <cstdint>
#include <d3d12.h>
#include <wrl.h>

class BloomRenderer {
public:
    struct BloomParameter {
        int32_t isEnabled;
        float threshold;
        int32_t blurRadius;
        float blurSigma;

        float intensity;
        int32_t blurDirection;
        float padding0;
        float padding1;
    };

    void Initialize(DirectXCommon* dxCommon);
    void Update();
    void DrawImGui();
    void Generate(D3D12_GPU_DESCRIPTOR_HANDLE sceneColorHandle);
    void Composite(D3D12_GPU_DESCRIPTOR_HANDLE sceneColorHandle);

    bool IsEnabled() const;

private:
    class RenderTarget {
    public:
        void Initialize(
            DirectXCommon* dxCommon,
            uint32_t rtvIndex,
            uint32_t width,
            uint32_t height,
            DXGI_FORMAT format);
        void BeginRender();
        void EndRender();
        D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandleGPU() const;

    private:
        void CreateResource();
        void CreateViews(uint32_t rtvIndex);
        void Transition(D3D12_RESOURCE_STATES nextState);

        DirectXCommon* dxCommon_ = nullptr;
        Microsoft::WRL::ComPtr<ID3D12Resource> resource_;
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle_ {};
        D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU_ {};
        D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU_ {};
        uint32_t srvIndex_ = 0;
        uint32_t width_ = 1;
        uint32_t height_ = 1;
        D3D12_RESOURCE_STATES currentState_ = D3D12_RESOURCE_STATE_RENDER_TARGET;
        D3D12_VIEWPORT viewport_ {};
        D3D12_RECT scissorRect_ {};
        DXGI_FORMAT format_ = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        float clearColor_[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    };

private:
    enum PipelineType {
        PipelineTypeBrightPass,
        PipelineTypeBlur,
        PipelineTypeComposite,
        PipelineTypeCount,
    };

    enum BloomRenderTargetType {
        BloomRenderTargetBrightPass,
        BloomRenderTargetBlurX,
        BloomRenderTargetBlurY,
        BloomRenderTargetCount,
    };

    void CreateRootSignature();
    void CreatePipelineStates();
    Microsoft::WRL::ComPtr<ID3D12PipelineState> CreatePipelineState(const std::wstring& pixelShaderPath);
    void CreateConstantBuffer();
    void SetDefaultParameter();
    void DrawFullScreen(
        PipelineType pipelineType,
        D3D12_GPU_DESCRIPTOR_HANDLE firstTextureHandle,
        D3D12_GPU_DESCRIPTOR_HANDLE secondTextureHandle);
    void ApplyBrightPass(D3D12_GPU_DESCRIPTOR_HANDLE sceneColorHandle);
    void ApplyBlur(
        D3D12_GPU_DESCRIPTOR_HANDLE sourceHandle,
        RenderTarget& destination,
        int32_t blurDirection);
    void ApplyComposite(
        D3D12_GPU_DESCRIPTOR_HANDLE sceneColorHandle,
        D3D12_GPU_DESCRIPTOR_HANDLE bloomColorHandle);

    static const uint32_t kFirstBloomRTVIndex = 5;
    static const uint32_t kBloomDownSample = 2;

    DirectXCommon* dxCommon_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    std::array<Microsoft::WRL::ComPtr<ID3D12PipelineState>, PipelineTypeCount> pipelineStates_;
    std::array<RenderTarget, BloomRenderTargetCount> renderTargets_;
    Microsoft::WRL::ComPtr<ID3D12Resource> parameterResource_;
    BloomParameter* parameterData_ = nullptr;
    DXGI_FORMAT format_ = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    D3D12_VIEWPORT fullScreenViewport_ {};
    D3D12_RECT fullScreenScissorRect_ {};
};

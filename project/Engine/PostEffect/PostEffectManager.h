#pragma once

#include "Engine/DirectXCommon/DirectXCommon.h"
#include "Engine/PostEffect/CopyImageRenderer.h"
#include <array>
#include <cstdint>
#include <memory>
#include <wrl.h>

class Camera;
class BloomRenderer;
class FogManager;
class FogRenderer;
class SceneManager;

class PostEffectManager {
public:
    PostEffectManager();
    ~PostEffectManager();

    void Initialize(DirectXCommon* dxCommon);
    void Update(Camera* camera);
    void DrawImGui();

    void PreDrawDepth();
    void PostDrawDepth();
    void PrepareDepthForParticleDraw();

    void SetBoostRadialBlurParameters(bool isBoosting);
    void Apply(SceneManager* sceneManager, D3D12_GPU_DESCRIPTOR_HANDLE sceneColorHandle);

    D3D12_CPU_DESCRIPTOR_HANDLE GetDepthDSVHandle() const;
    D3D12_GPU_VIRTUAL_ADDRESS GetFogConstantBufferView() const;
    CopyImageRenderer* GetCopyImageRenderer() const { return copyImageRenderer_.get(); }

private:
    class RenderTarget {
    public:
        void Initialize(DirectXCommon* dxCommon, uint32_t rtvIndex);
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
        D3D12_RESOURCE_STATES currentState_ = D3D12_RESOURCE_STATE_RENDER_TARGET;
        D3D12_VIEWPORT viewport_ {};
        D3D12_RECT scissorRect_ {};
        DXGI_FORMAT format_ = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        float clearColor_[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    };

private:
    void ApplyPostEffectToCurrentTarget(PostEffectType type, D3D12_GPU_DESCRIPTOR_HANDLE inputHandle);
    void SetBackBufferRenderTarget();
    uint32_t GetNextPingPongIndex(uint32_t currentIndex) const;

    static const uint32_t kPingPongRenderTargetCount = 2;
    static const uint32_t kFirstPingPongRTVIndex = 3;

    DirectXCommon* dxCommon_ = nullptr;
    std::unique_ptr<CopyImageRenderer> copyImageRenderer_;
    std::unique_ptr<BloomRenderer> bloomRenderer_;
    std::unique_ptr<FogManager> fogManager_;
    std::unique_ptr<FogRenderer> fogRenderer_;
    std::array<RenderTarget, kPingPongRenderTargetCount> pingPongRenderTargets_;
    bool isAnimationEnabled_ = true;
};

#pragma once
#if defined(ENABLE_DEVELOPMENT_TOOLS)
#include "externals/json.hpp"
#endif

#include "Engine/DirectXCommon/DirectXCommon.h"
#include "Engine/PostEffect/CopyImageRenderer.h"
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <wrl.h>

class Camera;
class BloomRenderer;
class VolumetricLightRenderer;
class FogManager;
class FogRenderer;
class SceneManager;

class PostEffectManager {
public:
    PostEffectManager();
    ~PostEffectManager();

    void Initialize(DirectXCommon* dxCommon);
    void Update(Camera* camera);
    bool SetSsao(bool isEnabled, float strength, float radius, float bias);
    void DrawImGui();
    void SetFxaaEnabled(bool enabled) { fxaaEnabled_ = enabled; }
    bool IsFxaaEnabled() const { return fxaaEnabled_; }
#if defined(ENABLE_DEVELOPMENT_TOOLS)
    void RegisterDevelopmentPanel();
    nlohmann::json GetDevelopmentControls() const;
    bool SetDevelopmentBool(const std::string& key, bool isEnabled);
    bool SetDevelopmentNumber(const std::string& key, double value);
    bool ExecuteDevelopmentCommand(const std::string& key);
    nlohmann::json GetDevelopmentSettings() const;
    std::string GetDevelopmentSettingsJson() const;
    bool ApplyDevelopmentSetting(const std::string& key, const std::string& value);
    void ClearDevelopmentPassOverrides() { passOverrides_.clear(); cameraShakeOverride_.reset(); }
#endif

    void PreDrawDepth();
    void PostDrawDepth();
    void PrepareDepthForParticleDraw();
    void SetNormalTextureHandle(D3D12_GPU_DESCRIPTOR_HANDLE handle)
    {
        normalTextureHandle_ = handle;
    }

    void Apply(SceneManager* sceneManager, D3D12_GPU_DESCRIPTOR_HANDLE sceneColorHandle);
    void PrepareSceneForParticleDraw(
        SceneManager* sceneManager,
        D3D12_GPU_DESCRIPTOR_HANDLE sceneColorHandle);
    void BeginParticleDraw();
    void EndParticleDraw();
    void ApplyAfterParticleDraw(SceneManager* sceneManager);

    D3D12_CPU_DESCRIPTOR_HANDLE GetDepthDSVHandle() const;
    D3D12_GPU_VIRTUAL_ADDRESS GetFogConstantBufferView() const;
    CopyImageRenderer* GetCopyImageRenderer() const { return copyImageRenderer_.get(); }
    BloomRenderer* GetBloomRenderer() const { return bloomRenderer_.get(); }
    VolumetricLightRenderer* GetVolumetricLightRenderer() const { return volumetricLightRenderer_.get(); }

private:
    class RenderTarget {
    public:
        void Initialize(DirectXCommon* dxCommon, uint32_t rtvIndex);
        void BeginRender();
        void BeginRenderWithDepth(
            D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle);
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
        DXGI_FORMAT format_ = DXGI_FORMAT_R16G16B16A16_FLOAT;
        float clearColor_[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    };

private:
    void FinishSceneColor(D3D12_GPU_DESCRIPTOR_HANDLE inputHandle);
    void ApplyPostEffectToCurrentTarget(PostEffectType type, D3D12_GPU_DESCRIPTOR_HANDLE inputHandle);
    void UpdatePostEffectParameters(SceneManager* sceneManager);
    std::unordered_map<PostEffectType, Vector3> defaultEffectParameters_;
    void SetBackBufferRenderTarget();
    uint32_t GetNextPingPongIndex(uint32_t currentIndex) const;

    static const uint32_t kPingPongRenderTargetCount = 2;
    static const uint32_t kFirstPingPongRTVIndex = 3;

    DirectXCommon* dxCommon_ = nullptr;
    std::unique_ptr<CopyImageRenderer> copyImageRenderer_;
    std::unique_ptr<BloomRenderer> bloomRenderer_;
    std::unique_ptr<VolumetricLightRenderer> volumetricLightRenderer_;
    bool sceneDepthReady_ = false;
    std::unique_ptr<FogManager> fogManager_;
    std::unique_ptr<FogRenderer> fogRenderer_;
    uint64_t sceneFogRevision_ = 0;
    uint64_t sceneExposureRevision_ = 0;
    std::array<RenderTarget, kPingPongRenderTargetCount> pingPongRenderTargets_;
    uint32_t particleCompositionTargetIndex_ = 0;
    bool isAnimationEnabled_ = true;
    bool fxaaEnabled_ = true;
#if defined(ENABLE_DEVELOPMENT_TOOLS)
    std::unordered_map<int, bool> passOverrides_;
    std::optional<float> cameraShakeOverride_;
#endif
    D3D12_GPU_DESCRIPTOR_HANDLE normalTextureHandle_ {};
};

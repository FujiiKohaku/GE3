#pragma once

#include <memory>
#include <cstdint>
#include <array>
#include "Engine/Debug/GpuTimestampTimer.h"
#if defined(ENABLE_DEVELOPMENT_TOOLS)
#include "externals/json.hpp"
#endif

class OffscreenRenderer;
class PostEffectManager;
class SceneManager;
class ShadowMapRenderer;
class LocalShadowRenderer;
class MotionVectorRenderer;
class DlssSuperResolution;
class ScreenSpaceReflection;
class ScreenSpaceGlobalIllumination;
class DxrRenderer;
class DxrShadowRenderer;
class DxrReflectionRenderer;
class DxrGlobalIlluminationRenderer;
class DxrLocalShadowRenderer;

class Renderer {
public:
    Renderer();
    ~Renderer();

    void Initialize();
    void Update();
    void DrawImGui();
    void Draw(SceneManager* sceneManager);
    PostEffectManager* GetPostEffectManager() const { return postEffectManager_.get(); }
    MotionVectorRenderer* GetMotionVectorRenderer() const { return motionVectorRenderer_.get(); }
    DlssSuperResolution* GetDlssSuperResolution() const { return dlssSuperResolution_.get(); }
    ScreenSpaceReflection* GetScreenSpaceReflection() const { return screenSpaceReflection_.get(); }
    ScreenSpaceGlobalIllumination* GetScreenSpaceGlobalIllumination() const { return screenSpaceGlobalIllumination_.get(); }
    DxrRenderer* GetDxrRenderer() const { return dxrRenderer_.get(); }
    DxrShadowRenderer* GetDxrShadowRenderer() const { return dxrShadowRenderer_.get(); }
    DxrReflectionRenderer* GetDxrReflectionRenderer() const { return dxrReflectionRenderer_.get(); }
    DxrGlobalIlluminationRenderer* GetDxrGlobalIlluminationRenderer() const { return dxrGlobalIlluminationRenderer_.get(); }
    DxrLocalShadowRenderer* GetDxrLocalShadowRenderer() const { return dxrLocalShadowRenderer_.get(); }
    void SetAntiAliasing(bool isDlaaEnabled, bool isFxaaEnabled);
    double GetFrameGpuTimeMs() const { return frameTimer_.GetDurationMs(); }
    double GetDlaaGpuTimeMs() const { return dlaaTimer_.GetDurationMs(); }
    bool HasGpuTimingSample() const { return frameTimer_.HasSample(); }
#if defined(ENABLE_DEVELOPMENT_TOOLS)
    nlohmann::json GetDevelopmentState() const;
    nlohmann::json GetDevelopmentControls() const;
    bool SetDevelopmentBool(const std::string& key, bool isEnabled);
    bool ExecuteDevelopmentCommand(const std::string& key);
#endif

private:
    struct AntiAliasingSample {
        uint32_t sampleCount = 0;
        double frameTotalMs = 0;
        double dlaaTotalMs = 0;
        double finalPassTotalMs = 0;
    };
    uint32_t GetAntiAliasingMode() const;
    void RecordGpuSample();
    GpuTimestampTimer frameTimer_;
    GpuTimestampTimer dlaaTimer_;
    std::array<AntiAliasingSample, 4> antiAliasingSamples_ = {};
    uint32_t warmupFrames_ = 32;
    uint32_t previousAntiAliasingMode_ = UINT_MAX;
    uint64_t comparisonSceneRevision_ = UINT64_MAX;
    std::unique_ptr<MotionVectorRenderer> motionVectorRenderer_;
    std::unique_ptr<DlssSuperResolution> dlssSuperResolution_;
    std::unique_ptr<ScreenSpaceReflection> screenSpaceReflection_;
    std::unique_ptr<ScreenSpaceGlobalIllumination> screenSpaceGlobalIllumination_;
    std::unique_ptr<DxrRenderer> dxrRenderer_;
    std::unique_ptr<DxrShadowRenderer> dxrShadowRenderer_;
    std::unique_ptr<DxrReflectionRenderer> dxrReflectionRenderer_;
    std::unique_ptr<DxrGlobalIlluminationRenderer> dxrGlobalIlluminationRenderer_;
    std::unique_ptr<DxrLocalShadowRenderer> dxrLocalShadowRenderer_;
    uint64_t previousLocalShadowSettingsRevision_ = 0;
    bool wasRtLocalShadowActive_ = false;
    bool wasRtGlobalIlluminationActive_ = false;
    uint64_t previousRtGlobalIlluminationSettingsRevision_ = 0;
    uint64_t previousGlobalIlluminationSettingsRevision_ = 0;
    uint64_t motionSceneRevision_ = 0;
    std::array<float, 4> previousLightingComponents_ = {-1, -1, -1, -1};
    std::unique_ptr<LocalShadowRenderer> localShadowRenderer_;
    std::unique_ptr<ShadowMapRenderer> shadowRenderer_;
    std::unique_ptr<OffscreenRenderer> offscreenRenderer_;
    std::unique_ptr<PostEffectManager> postEffectManager_;
};

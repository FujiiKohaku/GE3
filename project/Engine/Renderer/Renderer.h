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
    uint64_t motionSceneRevision_ = 0;
    std::unique_ptr<LocalShadowRenderer> localShadowRenderer_;
    std::unique_ptr<ShadowMapRenderer> shadowRenderer_;
    std::unique_ptr<OffscreenRenderer> offscreenRenderer_;
    std::unique_ptr<PostEffectManager> postEffectManager_;
};

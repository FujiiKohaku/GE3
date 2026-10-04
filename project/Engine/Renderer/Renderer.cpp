#include "Engine/Reflection/ScreenSpaceReflection.h"
#include "Engine/Lighting/ScreenSpaceGlobalIllumination.h"
#include "Engine/Renderer/Renderer.h"
#include "Engine/MotionVector/MotionVectorRenderer.h"
#include "Engine/SuperResolution/DlssSuperResolution.h"
#include "Engine/Time/TimeManager.h"
#include "Engine/Development/DevelopmentWebPanel.h"
#include <cmath>

#include "App/Scene/Common/SceneManager.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/Camera/Camera.h"
#include "Engine/Debug/DebugRenderer.h"
#include "Engine/DirectXCommon/DirectXCommon.h"
#include "Engine/Effect/EffectManager.h"
#include "Engine/2D/Text/FontManager.h"
#include "Engine/ImGuiManager/ImGuiManager.h"
#include "Engine/PostEffect/OffscreenRenderer.h"
#include "Engine/PostEffect/PostEffectManager.h"
#include "Engine/SrvManager/SrvManager.h"
#include "Engine/Screenshot/ScreenshotManager.h"
#include "Engine/TextureManager/TextureManager.h"
#include "Engine/Shadow/ShadowMapRenderer.h"
#include "Engine/Shadow/LocalShadowRenderer.h"
#include "Engine/Light/LightManager.h"
#include "Engine/PostEffect/Volumetric/VolumetricLightRenderer.h"

Renderer::Renderer() = default;

Renderer::~Renderer()
{
#if defined(ENABLE_DEVELOPMENT_TOOLS)
    DevelopmentWebPanel::GetInstance().UnregisterOwner(this);
#endif
    Object3dManager::GetInstance()->SetShadowRenderer(nullptr);
    Object3dManager::GetInstance()->SetLocalShadowRenderer(nullptr);
}

void Renderer::Initialize()
{
    // Offscreen renderer setup
    offscreenRenderer_ = std::make_unique<OffscreenRenderer>();
    offscreenRenderer_->Initialize();
    motionVectorRenderer_ = std::make_unique<MotionVectorRenderer>();
    motionVectorRenderer_->Initialize();

    // Post effect chain setup
    postEffectManager_ = std::make_unique<PostEffectManager>();
    postEffectManager_->Initialize(DirectXCommon::GetInstance());
    postEffectManager_->SetNormalTextureHandle(
        offscreenRenderer_->GetNormalSrvHandleGPU());
    postEffectManager_->SetIndirectTextureHandle(offscreenRenderer_->GetIndirectSrvHandleGPU());
    dlssSuperResolution_ = std::make_unique<DlssSuperResolution>();
    dlssSuperResolution_->Initialize();
    screenSpaceReflection_ = std::make_unique<ScreenSpaceReflection>();
    screenSpaceReflection_->Initialize();
    screenSpaceGlobalIllumination_ = std::make_unique<ScreenSpaceGlobalIllumination>();
    screenSpaceGlobalIllumination_->Initialize();
    frameTimer_.Initialize();
    dlaaTimer_.Initialize();
#if defined(ENABLE_DEVELOPMENT_TOOLS)
    DevelopmentWebPanel::GetInstance().RegisterSource<Renderer>(this, "antialiasing", "アンチエイリアス比較", false,
        &Renderer::GetDevelopmentState, &Renderer::GetDevelopmentControls, &Renderer::SetDevelopmentBool,
        nullptr, &Renderer::ExecuteDevelopmentCommand);
#endif
}

void Renderer::Update()
{
    Camera* defaultCamera = Object3dManager::GetInstance()->GetDefaultCamera();
    postEffectManager_->Update(defaultCamera);
}

void Renderer::DrawImGui()
{
    postEffectManager_->DrawImGui();
    screenSpaceReflection_->DrawImGui();
    screenSpaceGlobalIllumination_->DrawImGui();
#ifdef USE_IMGUI
    if (ImGui::Begin("Motion vectors")) {
        MotionVectorSettings settings = motionVectorRenderer_->GetSettings();
        ImGui::Checkbox("Enabled", &settings.isEnabled);
        ImGui::Checkbox("Visualize UV displacement", &settings.isDebugVisible);
        motionVectorRenderer_->SetSettings(settings);
        if (ImGui::Button("Reset history")) { motionVectorRenderer_->ResetHistory(); screenSpaceReflection_->ResetHistory(); screenSpaceGlobalIllumination_->ResetHistory(); }
    }
    ImGui::End();
    ImGui::SetNextWindowSize(ImVec2(570, 390), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Anti-aliasing comparison")) {
        if (ImGui::Button("OFF")) { SetAntiAliasing(false, false); }
        ImGui::SameLine();
        if (ImGui::Button("FXAA")) { SetAntiAliasing(false, true); }
        ImGui::SameLine();
        if (ImGui::Button("DLAA")) { SetAntiAliasing(true, false); }
        ImGui::SameLine();
        if (ImGui::Button("DLAA + FXAA")) { SetAntiAliasing(true, true); }
        bool isEnabled = dlssSuperResolution_->IsEnabled();
        if (ImGui::Checkbox("DLAA (Native resolution)", &isEnabled)) {
            SetAntiAliasing(isEnabled, postEffectManager_->IsFxaaEnabled());
        }
        bool isFxaaEnabled = postEffectManager_->IsFxaaEnabled();
        if (ImGui::Checkbox("FXAA", &isFxaaEnabled)) { SetAntiAliasing(isEnabled, isFxaaEnabled); }
        ImGui::TextWrapped("%s", dlssSuperResolution_->GetStatus().c_str());
        if (!frameTimer_.IsAvailable()) { ImGui::TextUnformatted("GPU timing unavailable"); }
        else {
            ImGui::Text("GPU draw: %.3f ms", GetFrameGpuTimeMs());
            ImGui::Text("DLAA + copy: %.3f ms", GetDlaaGpuTimeMs());
            ImGui::Text("Tone map / FXAA: %.3f ms", postEffectManager_->GetFinalPassGpuTimeMs());
            ImGui::Text("Warmup: %u frames remaining", warmupFrames_);
            const char* kModeNames[] = {"OFF", "DLAA", "FXAA", "DLAA + FXAA"};
            for (uint32_t index = 0; index < antiAliasingSamples_.size(); ++index) {
                const auto& sample = antiAliasingSamples_[index];
                if (sample.sampleCount == 0) { ImGui::Text("%s: no samples", kModeNames[index]); continue; }
                ImGui::Text("%s (%u): frame %.3f / DLAA %.3f / final %.3f ms", kModeNames[index], sample.sampleCount,
                    sample.frameTotalMs / sample.sampleCount, sample.dlaaTotalMs / sample.sampleCount,
                    sample.finalPassTotalMs / sample.sampleCount);
            }
            ImGui::TextWrapped("Same camera/scene for comparison. Final pass includes tone mapping; DLAA includes the composition copy. GPU draw excludes update commands, CPU and Present wait.");
        }
    }
    ImGui::End();
#endif
}

void Renderer::Draw(SceneManager* sceneManager)
{
    if (comparisonSceneRevision_ != sceneManager->GetSceneRevision()) {
        antiAliasingSamples_ = {};
        warmupFrames_ = 32;
        comparisonSceneRevision_ = sceneManager->GetSceneRevision();
    }
    frameTimer_.Begin();
    FontManager::GetInstance()->FlushAtlasUpdates();
    // シーンやモデルが予約したテクスチャ転送を、描画前に一度だけまとめて実行する。
    TextureManager::GetInstance()->FlushUploads();

    // SRV heap setup
    SrvManager::GetInstance()->PreDraw();

    Camera* defaultCamera = Object3dManager::GetInstance()->GetDefaultCamera();
    Vector4 lightingComponents = LightManager::GetInstance()->GetLightingComponents();
    const std::array<float, 4> kCurrentLightingComponents = {lightingComponents.x, lightingComponents.y, lightingComponents.z, lightingComponents.w};
    if (previousLightingComponents_ != kCurrentLightingComponents) {
        dlssSuperResolution_->ResetHistory();
        screenSpaceReflection_->ResetHistory();
        screenSpaceGlobalIllumination_->ResetHistory();
        previousLightingComponents_ = kCurrentLightingComponents;
    }
    uint64_t globalIlluminationSettingsRevision = screenSpaceGlobalIllumination_->GetSettingsRevision();
    if (previousGlobalIlluminationSettingsRevision_ != globalIlluminationSettingsRevision) {
        dlssSuperResolution_->ResetHistory();
        screenSpaceReflection_->ResetHistory();
        previousGlobalIlluminationSettingsRevision_ = globalIlluminationSettingsRevision;
    }
    SuperResolutionHistoryInputs historyInputs;
    historyInputs.hasCamera = defaultCamera != nullptr;
    historyInputs.cameraId = reinterpret_cast<uintptr_t>(defaultCamera);
    historyInputs.sceneRevision = sceneManager->GetSceneRevision();
    if (defaultCamera != nullptr) { historyInputs.cameraHistoryId = defaultCamera->GetMotionHistoryId(); }
    dlssSuperResolution_->BeginFrame(historyInputs);
    if (defaultCamera != nullptr) { defaultCamera->SetProjectionJitter(dlssSuperResolution_->GetProjectionJitterNdc()); }
    postEffectManager_->UpdateCameraInputs(defaultCamera);
    const auto motionSettings = motionVectorRenderer_->GetSettings();
    if (dlssSuperResolution_->IsActive() ||
        (screenSpaceReflection_->IsEnabled() && screenSpaceReflection_->GetSettings().shouldUseTemporalHistory) ||
        (screenSpaceGlobalIllumination_->IsEnabled() && screenSpaceGlobalIllumination_->GetSettings().shouldUseTemporalHistory)) {
        auto settings = motionVectorRenderer_->GetSettings();
        settings.isEnabled = true;
        motionVectorRenderer_->SetSettings(settings);
    }
    SrvManager::GetInstance()->PreDraw();
    LightManager::GetInstance()->UpdateClusters(defaultCamera);
    EffectManager* effectManager = EffectManager::GetInstance();
    if (effectManager->IsInitialized()) {
        if (defaultCamera != nullptr) {
            effectManager->SetCamera(defaultCamera);
        }

        D3D12_GPU_VIRTUAL_ADDRESS fogConstantBufferView =
            postEffectManager_->GetFogConstantBufferView();
        effectManager->SetFogConstantBufferView(fogConstantBufferView);
    }

    ShadowSettings shadows = sceneManager->GetShadowSettings();
    Object3dManager::GetInstance()->SetShadowRenderer(nullptr);
    if (shadows.enabled && defaultCamera != nullptr) {
        if (!shadowRenderer_ || shadowRenderer_->GetResolution() != shadows.resolution) {
            DirectXCommon::GetInstance()->WaitForGPU();
            shadowRenderer_ = std::make_unique<ShadowMapRenderer>();
            shadowRenderer_->Initialize(DirectXCommon::GetInstance(), shadows.resolution);
        }
        shadowRenderer_->Update(*defaultCamera, LightManager::GetInstance()->GetDirectionalDirection(), shadows);
        shadowRenderer_->BeginShadowPass();
        sceneManager->DrawShadow(*shadowRenderer_);
        shadowRenderer_->EndShadowPass();
        Object3dManager::GetInstance()->SetShadowRenderer(shadowRenderer_.get());
    }

    Object3dManager::GetInstance()->SetLocalShadowRenderer(nullptr);
    bool hasLocalShadows = false;
    auto* lights = LightManager::GetInstance();
    for (uint32_t lightIndex = 0; lightIndex < LightManager::kMaxPointLights; ++lightIndex) {
        const PointLight light = lights->GetPointLight(lightIndex);
        if (lights->IsPointLightShadowEnabled(lightIndex) && light.isActive != 0 && light.intensity > 0.0f) { hasLocalShadows = true; }
    }
    for (uint32_t lightIndex = 0; lightIndex < LightManager::kMaxSpotLights; ++lightIndex) {
        const SpotLight light = lights->GetSpotLight(lightIndex);
        if (lights->IsSpotLightShadowEnabled(lightIndex) && light.isActive != 0 && light.intensity > 0.0f) { hasLocalShadows = true; }
    }
    if (hasLocalShadows && defaultCamera != nullptr) {
        if (!localShadowRenderer_) {
            localShadowRenderer_ = std::make_unique<LocalShadowRenderer>();
            localShadowRenderer_->Initialize(DirectXCommon::GetInstance());
        }
        localShadowRenderer_->Prepare(*lights);
        for (uint32_t passIndex = 0; passIndex < localShadowRenderer_->GetPassCount(); ++passIndex) {
            ShadowMapRenderer& pass = localShadowRenderer_->GetPass(passIndex);
            pass.BeginShadowPass();
            sceneManager->DrawShadow(pass);
            pass.EndShadowPass();
        }
        localShadowRenderer_->Finish();
        if (localShadowRenderer_->HasValidFrame()) { Object3dManager::GetInstance()->SetLocalShadowRenderer(localShadowRenderer_.get()); }
    }

    // Always clear/supply frame inputs, including frames with shadows disabled.
    const ShadowMapRenderer* volumetricShadows = nullptr;
    if (shadows.enabled) { volumetricShadows = shadowRenderer_.get(); }
    const LocalShadowRenderer* volumetricLocalShadows = nullptr;
    if (hasLocalShadows && localShadowRenderer_ && localShadowRenderer_->HasValidFrame()) {
        volumetricLocalShadows = localShadowRenderer_.get();
    }
    postEffectManager_->GetVolumetricLightRenderer()->SetFrameInputs(
        defaultCamera, volumetricShadows, volumetricLocalShadows);

    // Offscreen draw start
    postEffectManager_->PreDrawDepth();
    offscreenRenderer_->SetClearColor(sceneManager->GetSceneClearColor());
    offscreenRenderer_->PreDraw(postEffectManager_->GetDepthDSVHandle());
    dlssSuperResolution_->SetSceneViewport();
    if (motionSceneRevision_ != sceneManager->GetSceneRevision()) {
        motionVectorRenderer_->ResetHistory();
        motionSceneRevision_ = sceneManager->GetSceneRevision();
    }
    motionVectorRenderer_->BeginFrame();
    sceneManager->Draw3D();
    DebugRenderer::GetInstance()->Draw();
    motionVectorRenderer_->EndFrame(postEffectManager_->GetDepthDSVHandle());
    motionVectorRenderer_->SetSettings(motionSettings);
    postEffectManager_->PostDrawDepth();
    offscreenRenderer_->PostDraw();
    DirectXCommon::GetInstance()->PreDraw();
    ScreenSpaceGlobalIlluminationInputs globalIlluminationInputs;
    globalIlluminationInputs.colorSrv = offscreenRenderer_->GetSrvHandleGPU();
    globalIlluminationInputs.depthSrv = postEffectManager_->GetDepthSrv();
    globalIlluminationInputs.normalSrv = offscreenRenderer_->GetNormalSrvHandleGPU();
    globalIlluminationInputs.materialSrv = offscreenRenderer_->GetMaterialSrvHandleGPU();
    globalIlluminationInputs.motionVectorSrv = motionVectorRenderer_->GetSrvHandle();
    globalIlluminationInputs.camera = defaultCamera;
    globalIlluminationInputs.sceneRevision = sceneManager->GetSceneRevision();
    bool isLightingComponentView = lights->GetLightingComponents().w > 0.5f;
    if (isLightingComponentView) { globalIlluminationInputs.camera = nullptr; }
    if (screenSpaceGlobalIllumination_->IsEnabled() &&
        screenSpaceGlobalIllumination_->GetSettings().shouldUseHierarchicalDepth && !isLightingComponentView) {
        ScreenSpaceReflectionInputs depthInputs;
        depthInputs.colorSrv = globalIlluminationInputs.colorSrv;
        depthInputs.depthSrv = globalIlluminationInputs.depthSrv;
        depthInputs.normalSrv = globalIlluminationInputs.normalSrv;
        depthInputs.camera = defaultCamera;
        screenSpaceReflection_->PrepareDepthPyramid(depthInputs);
        globalIlluminationInputs.depthPyramidSrv = screenSpaceReflection_->GetDepthPyramidSrv();
    }
    D3D12_GPU_DESCRIPTOR_HANDLE sceneWithIndirectLight = screenSpaceGlobalIllumination_->Draw(globalIlluminationInputs);
    bool isGlobalIlluminationDebugView = screenSpaceGlobalIllumination_->IsEnabled() &&
        screenSpaceGlobalIllumination_->GetSettings().debugMode != ScreenSpaceGlobalIlluminationDebugMode::None;
    postEffectManager_->SetIndirectLightingDebugVisible(isGlobalIlluminationDebugView);
    postEffectManager_->PrepareSceneForTemporalResolve(sceneManager, sceneWithIndirectLight);

    ScreenSpaceReflectionInputs reflectionInputs;
    reflectionInputs.colorSrv = postEffectManager_->GetSceneColorSrv();
    reflectionInputs.depthSrv = postEffectManager_->GetDepthSrv();
    reflectionInputs.normalSrv = offscreenRenderer_->GetNormalSrvHandleGPU();
    reflectionInputs.motionVectorSrv = motionVectorRenderer_->GetSrvHandle();
    reflectionInputs.sceneRevision = sceneManager->GetSceneRevision();
    reflectionInputs.camera = defaultCamera;
    // Reflection is a separate lighting contribution; isolate the selected source view.
    if (isLightingComponentView || isGlobalIlluminationDebugView) { reflectionInputs.camera = nullptr; }
    postEffectManager_->ReplaceSceneColor(screenSpaceReflection_->Draw(reflectionInputs));

    SuperResolutionFrameInputs frameInputs;
    frameInputs.colorTexture = postEffectManager_->GetSceneColorTexture();
    frameInputs.depthTexture = postEffectManager_->GetDepthTexture();
    frameInputs.motionVectorTexture = motionVectorRenderer_->GetTexture();
    frameInputs.colorSrv = postEffectManager_->GetSceneColorSrv();
    frameInputs.frameTimeDeltaMs = TimeManager::GetInstance()->GetUnscaledDeltaTime() * 1000;
    bool isDlaaActive = dlssSuperResolution_->IsActive();
    dlaaTimer_.ResetSample();
    if (isDlaaActive) { dlaaTimer_.Begin(); }
    D3D12_GPU_DESCRIPTOR_HANDLE sceneColorHandle = dlssSuperResolution_->Evaluate(frameInputs);
    postEffectManager_->ReplaceSceneColor(sceneColorHandle);
    if (isDlaaActive) { dlaaTimer_.End(); }
    if (defaultCamera != nullptr) { defaultCamera->SetProjectionJitter({}); }
    postEffectManager_->UpdateCameraInputs(defaultCamera);
    if (effectManager->IsInitialized() && defaultCamera != nullptr) { effectManager->UpdatePerView(); }

    postEffectManager_->PrepareDepthForParticleDraw();
    postEffectManager_->BeginParticleDraw();
    sceneManager->DrawParticle();
    postEffectManager_->EndParticleDraw();
    postEffectManager_->PostDrawDepth();

    postEffectManager_->ApplyAfterParticleDraw(sceneManager);
    motionVectorRenderer_->DrawDebug();

    // 2D draw
    sceneManager->Draw2D();

    // ImGui is not initialized in the Release configuration.
#ifdef USE_IMGUI
    ImGuiManager::GetInstance()->Draw();
#endif

    ScreenshotManager::GetInstance()->DrawNotification();
    ScreenshotManager::GetInstance()->PrepareCapture();

    // Present
    frameTimer_.End();
    DirectXCommon::GetInstance()->PostDraw();
    // PostDraw waits for its GPU fence before resetting the command list.
    frameTimer_.ReadCompleted();
    dlaaTimer_.ReadCompleted();
    screenSpaceReflection_->ReadCompleted();
    screenSpaceGlobalIllumination_->ReadCompleted();
    postEffectManager_->ReadCompletedGpuTiming();
    RecordGpuSample();
    ScreenshotManager::GetInstance()->CompleteCapture();
}

void Renderer::SetAntiAliasing(bool isDlaaEnabled, bool isFxaaEnabled)
{
    if (dlssSuperResolution_->IsEnabled() == isDlaaEnabled && postEffectManager_->IsFxaaEnabled() == isFxaaEnabled) { return; }
    dlssSuperResolution_->SetEnabled(isDlaaEnabled);
    postEffectManager_->SetFxaaEnabled(isFxaaEnabled);
    dlssSuperResolution_->ResetHistory();
    motionVectorRenderer_->ResetHistory();
    screenSpaceReflection_->ResetHistory();
    screenSpaceGlobalIllumination_->ResetHistory();
    warmupFrames_ = 32;
    previousAntiAliasingMode_ = UINT_MAX;
}

uint32_t Renderer::GetAntiAliasingMode() const
{
    uint32_t mode = 0;
    if (dlssSuperResolution_->IsEnabled()) { mode += 1; }
    if (postEffectManager_->IsFxaaEnabled()) { mode += 2; }
    return mode;
}

void Renderer::RecordGpuSample()
{
    uint32_t mode = GetAntiAliasingMode();
    if (previousAntiAliasingMode_ != mode) {
        antiAliasingSamples_[mode] = {};
        previousAntiAliasingMode_ = mode;
        warmupFrames_ = 32;
    }
    if (!frameTimer_.HasSample()) { return; }
    if (warmupFrames_ > 0) { --warmupFrames_; return; }
    if (dlssSuperResolution_->IsEnabled() && !dlssSuperResolution_->IsActive()) { return; }
    auto& sample = antiAliasingSamples_[mode];
    ++sample.sampleCount;
    sample.frameTotalMs += GetFrameGpuTimeMs();
    sample.dlaaTotalMs += GetDlaaGpuTimeMs();
    sample.finalPassTotalMs += postEffectManager_->GetFinalPassGpuTimeMs();
}

#if defined(ENABLE_DEVELOPMENT_TOOLS)
nlohmann::json Renderer::GetDevelopmentState() const
{
    nlohmann::json averages = nlohmann::json::array();
    const char* kModeNames[] = {"OFF", "DLAA", "FXAA", "DLAA + FXAA"};
    for (uint32_t index = 0; index < antiAliasingSamples_.size(); ++index) {
        const auto& sample = antiAliasingSamples_[index];
        double frameMs = 0, dlaaMs = 0, finalMs = 0;
        if (sample.sampleCount > 0) {
            frameMs = sample.frameTotalMs / sample.sampleCount;
            dlaaMs = sample.dlaaTotalMs / sample.sampleCount;
            finalMs = sample.finalPassTotalMs / sample.sampleCount;
        }
        averages.push_back({{"mode", kModeNames[index]}, {"samples", sample.sampleCount},
            {"frameMs", frameMs}, {"dlaaMs", dlaaMs}, {"finalPassMs", finalMs}});
    }
    return {{"dlaaEnabled", dlssSuperResolution_->IsEnabled()}, {"fxaaEnabled", postEffectManager_->IsFxaaEnabled()},
        {"dlaaActive", dlssSuperResolution_->IsActive()}, {"status", dlssSuperResolution_->GetStatus()},
        {"gpuTimingAvailable", frameTimer_.IsAvailable()}, {"warmupFrames", warmupFrames_},
        {"frameGpuMs", std::round(GetFrameGpuTimeMs() * 1000) / 1000},
        {"dlaaGpuMs", std::round(GetDlaaGpuTimeMs() * 1000) / 1000},
        {"ssrGpuMs", screenSpaceReflection_->GetGpuTimeMs()}, {"ssrEnabled", screenSpaceReflection_->IsEnabled()},
        {"finalPassGpuMs", std::round(postEffectManager_->GetFinalPassGpuTimeMs() * 1000) / 1000}, {"averages", averages}};
}

nlohmann::json Renderer::GetDevelopmentControls() const
{
    return nlohmann::json::array({
        {{"key", "ssrEnabled"}, {"label", "床のSSRを有効"}, {"type", "bool"}},
        {{"key", "ssrGpuMs"}, {"label", "SSR 探索 + 合成 (ms)"}, {"type", "metric"}},
        {{"key", "aaOff"}, {"label", "AAなし"}, {"type", "action"}},
        {{"key", "aaFxaa"}, {"label", "FXAAのみ"}, {"type", "action"}},
        {{"key", "aaDlaa"}, {"label", "DLAAのみ"}, {"type", "action"}},
        {{"key", "aaBoth"}, {"label", "DLAA + FXAA"}, {"type", "action"}},
        {{"key", "dlaaEnabled"}, {"label", "DLAAを有効"}, {"type", "bool"}},
        {{"key", "fxaaEnabled"}, {"label", "FXAAを有効"}, {"type", "bool"}},
        {{"key", "status"}, {"label", "DLAA状態"}, {"type", "metric"}},
        {{"key", "frameGpuMs"}, {"label", "描画区間 GPU (ms)"}, {"type", "metric"}},
        {{"key", "dlaaGpuMs"}, {"label", "DLAA + 合成コピー (ms)"}, {"type", "metric"}},
        {{"key", "finalPassGpuMs"}, {"label", "トーンマッピング / FXAA (ms)"}, {"type", "metric"}},
        {{"key", "warmupFrames"}, {"label", "平均計測までの残りフレーム"}, {"type", "metric"}}
    });
}

bool Renderer::SetDevelopmentBool(const std::string& key, bool isEnabled)
{
    if (key == "ssrEnabled") { screenSpaceReflection_->SetEnabled(isEnabled); return true; }
    if (key == "dlaaEnabled") { SetAntiAliasing(isEnabled, postEffectManager_->IsFxaaEnabled()); return true; }
    if (key == "fxaaEnabled") { SetAntiAliasing(dlssSuperResolution_->IsEnabled(), isEnabled); return true; }
    return false;
}

bool Renderer::ExecuteDevelopmentCommand(const std::string& key)
{
    if (key == "aaOff") { SetAntiAliasing(false, false); return true; }
    if (key == "aaFxaa") { SetAntiAliasing(false, true); return true; }
    if (key == "aaDlaa") { SetAntiAliasing(true, false); return true; }
    if (key == "aaBoth") { SetAntiAliasing(true, true); return true; }
    return false;
}
#endif

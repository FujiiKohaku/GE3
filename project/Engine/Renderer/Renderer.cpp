#include "SceneRenderResolution.h"
#include "Engine/Reflection/ScreenSpaceReflection.h"
#include "Engine/Raytracing/DxrRenderer.h"
#include "Engine/Raytracing/DxrShadowRenderer.h"
#include "Engine/Raytracing/DxrReflectionRenderer.h"
#include "Engine/Raytracing/DxrGlobalIlluminationRenderer.h"
#include "Engine/Raytracing/DxrLocalShadowRenderer.h"
#include "Engine/Lighting/ScreenSpaceGlobalIllumination.h"
#include "Engine/Renderer/Renderer.h"
#include "Engine/MotionVector/MotionVectorRenderer.h"
#include "Engine/SuperResolution/DlssSuperResolution.h"
#include "Engine/SuperResolution/TemporalSuperResolution.h"
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
    SceneRenderResolution::SetSize(WinApp::kClientWidth, WinApp::kClientHeight);
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
    temporalSuperResolution_ = std::make_unique<TemporalSuperResolution>();
    dlssSuperResolution_ = std::make_unique<DlssSuperResolution>();
    dlssSuperResolution_->Initialize();
    screenSpaceReflection_ = std::make_unique<ScreenSpaceReflection>();
    screenSpaceReflection_->Initialize();
    screenSpaceGlobalIllumination_ = std::make_unique<ScreenSpaceGlobalIllumination>();
    screenSpaceGlobalIllumination_->Initialize();
    dxrRenderer_ = std::make_unique<DxrRenderer>();
    dxrRenderer_->Initialize();
    dxrShadowRenderer_ = std::make_unique<DxrShadowRenderer>();
    dxrShadowRenderer_->Initialize();
    dxrReflectionRenderer_ = std::make_unique<DxrReflectionRenderer>();
    dxrReflectionRenderer_->Initialize();
    dxrGlobalIlluminationRenderer_ = std::make_unique<DxrGlobalIlluminationRenderer>();
    dxrGlobalIlluminationRenderer_->Initialize();
    dxrLocalShadowRenderer_ = std::make_unique<DxrLocalShadowRenderer>(); dxrLocalShadowRenderer_->Initialize();
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
    dxrRenderer_->DrawImGui();
    dxrShadowRenderer_->DrawImGui();
    dxrReflectionRenderer_->DrawImGui();
    dxrGlobalIlluminationRenderer_->DrawImGui();
    dxrLocalShadowRenderer_->DrawImGui();
#ifdef USE_IMGUI
    if (ImGui::Begin("Motion vectors")) {
        MotionVectorSettings settings = motionVectorRenderer_->GetSettings();
        ImGui::Checkbox("Enabled", &settings.isEnabled);
        ImGui::Checkbox("Visualize UV displacement", &settings.isDebugVisible);
        motionVectorRenderer_->SetSettings(settings);
        if (ImGui::Button("Reset history")) {
            motionVectorRenderer_->ResetHistory(); temporalSuperResolution_->ResetHistory(); screenSpaceReflection_->ResetHistory(); screenSpaceGlobalIllumination_->ResetHistory();
            dxrGlobalIlluminationRenderer_->ResetHistory(); dxrReflectionRenderer_->ResetHistory();
            dxrLocalShadowRenderer_->ResetHistory();
        }
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
        if (ImGui::Button("Quality")) { SetRenderQualityPreset(RenderQualityPreset::Quality); }
        ImGui::SameLine();
        if (ImGui::Button("Balanced")) { SetRenderQualityPreset(RenderQualityPreset::Balanced); }
        ImGui::SameLine();
        if (ImGui::Button("Performance")) { SetRenderQualityPreset(RenderQualityPreset::Performance); }
        const char* kQualityNames[] = {"Quality", "Balanced", "Performance", "Custom"};
        ImGui::Text("RT / fog quality: %s", kQualityNames[static_cast<uint32_t>(GetRenderQualityPreset())]);
        if (ImGui::Button("Apply recommended resolution")) { ApplyRecommendedRenderResolution(); }
        bool isLowResolutionEnabled = isLowResolutionRenderingEnabled_;
        if (ImGui::Checkbox("Low resolution rendering", &isLowResolutionEnabled)) {
            SetLowResolutionRendering(isLowResolutionEnabled, lowResolutionWidth_, lowResolutionHeight_);
        }
        if (ImGui::Button("Render 960 x 540")) { SetLowResolutionRendering(true, 960, 540); }
        ImGui::SameLine();
        if (ImGui::Button("Render 640 x 360")) { SetLowResolutionRendering(true, 640, 360); }
        ImGui::Text("Scene %u x %u -> display 1280 x 720", sceneRenderWidth_, sceneRenderHeight_);
        if (isLowResolutionRenderingEnabled_ && !temporalSuperResolution_->GetSettings().isEnabled) { ImGui::TextUnformatted("Spatial upscale (TAA OFF)"); }
        if (ImGui::Button("TAA")) { SetTemporalAntiAliasing(true, false); }
        ImGui::SameLine();
        if (ImGui::Button("TAA + FXAA")) { SetTemporalAntiAliasing(true, true); }
        ImGui::Text("TAA: %.3f ms / %.2f MiB", temporalSuperResolution_->GetGpuTimeMs(), temporalSuperResolution_->GetAllocationBytes() / 1048576.0);
        bool isEnabled = dlssSuperResolution_->IsEnabled();
        if (ImGui::Checkbox("DLAA (Native resolution)", &isEnabled)) {
            SetAntiAliasing(isEnabled, postEffectManager_->IsFxaaEnabled());
        }
        bool isFxaaEnabled = postEffectManager_->IsFxaaEnabled();
        if (ImGui::Checkbox("FXAA", &isFxaaEnabled)) {
            if (temporalSuperResolution_->GetSettings().isEnabled) { SetTemporalAntiAliasing(true, isFxaaEnabled); }
            else { SetAntiAliasing(isEnabled, isFxaaEnabled); }
        }
        ImGui::TextWrapped("%s", dlssSuperResolution_->GetStatus().c_str());
        if (!frameTimer_.IsAvailable()) { ImGui::TextUnformatted("GPU timing unavailable"); }
        else {
            ImGui::Text("GPU draw: %.3f ms", GetFrameGpuTimeMs());
            ImGui::Text("DLAA + copy: %.3f ms", GetDlaaGpuTimeMs());
            ImGui::Text("Tone map / FXAA: %.3f ms", postEffectManager_->GetFinalPassGpuTimeMs());
            ImGui::Text("Warmup: %u frames remaining", warmupFrames_);
            const char* kModeNames[] = {"OFF", "DLAA", "FXAA", "DLAA + FXAA", "TAA", "TAA + FXAA"};
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
    ApplySceneRenderResolution();
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
        dlssSuperResolution_->ResetHistory(); temporalSuperResolution_->ResetHistory();
        screenSpaceReflection_->ResetHistory();
        screenSpaceGlobalIllumination_->ResetHistory();
        previousLightingComponents_ = kCurrentLightingComponents;
    }
    uint64_t globalIlluminationSettingsRevision = screenSpaceGlobalIllumination_->GetSettingsRevision();
    if (previousGlobalIlluminationSettingsRevision_ != globalIlluminationSettingsRevision) {
        dlssSuperResolution_->ResetHistory(); temporalSuperResolution_->ResetHistory();
        screenSpaceReflection_->ResetHistory();
        previousGlobalIlluminationSettingsRevision_ = globalIlluminationSettingsRevision;
    }
    uint64_t rtGlobalIlluminationSettingsRevision = dxrGlobalIlluminationRenderer_->GetSettingsRevision();
    uint64_t localShadowSettingsRevision = dxrLocalShadowRenderer_->GetSettingsRevision();
    if (previousLocalShadowSettingsRevision_ != localShadowSettingsRevision) {
        dlssSuperResolution_->ResetHistory(); temporalSuperResolution_->ResetHistory(); screenSpaceReflection_->ResetHistory();
        dxrGlobalIlluminationRenderer_->ResetHistory(); dxrReflectionRenderer_->ResetHistory();
        previousLocalShadowSettingsRevision_ = localShadowSettingsRevision;
    }
    if (previousRtGlobalIlluminationSettingsRevision_ != rtGlobalIlluminationSettingsRevision) {
        dlssSuperResolution_->ResetHistory(); temporalSuperResolution_->ResetHistory(); screenSpaceReflection_->ResetHistory(); dxrReflectionRenderer_->ResetHistory();
        previousRtGlobalIlluminationSettingsRevision_ = rtGlobalIlluminationSettingsRevision;
    }
    SuperResolutionHistoryInputs historyInputs;
    historyInputs.hasCamera = defaultCamera != nullptr;
    historyInputs.cameraId = reinterpret_cast<uintptr_t>(defaultCamera);
    historyInputs.sceneRevision = sceneManager->GetSceneRevision();
    if (defaultCamera != nullptr) { historyInputs.cameraHistoryId = defaultCamera->GetMotionHistoryId(); }
    historyInputs.radianceRevision = UpdateTemporalRadianceRevision();
    SuperResolutionHistoryInputs dlaaHistoryInputs = historyInputs;
    if (temporalSuperResolution_->GetSettings().isEnabled || isLowResolutionRenderingEnabled_) { dlaaHistoryInputs.hasCamera = false; }
    dlssSuperResolution_->BeginFrame(dlaaHistoryInputs);
    temporalSuperResolution_->BeginFrame(historyInputs);
    Vector2 projectionJitter = dlssSuperResolution_->GetProjectionJitterNdc();
    if (temporalSuperResolution_->IsActive()) { projectionJitter = temporalSuperResolution_->GetProjectionJitterNdc(); }
    if (defaultCamera != nullptr) { defaultCamera->SetProjectionJitter(projectionJitter); }
    postEffectManager_->UpdateCameraInputs(defaultCamera);
    const auto motionSettings = motionVectorRenderer_->GetSettings();
    if (dlssSuperResolution_->IsActive() || temporalSuperResolution_->IsActive() ||
        (screenSpaceReflection_->IsEnabled() && screenSpaceReflection_->GetSettings().shouldUseTemporalHistory) ||
        (screenSpaceGlobalIllumination_->IsEnabled() && screenSpaceGlobalIllumination_->GetSettings().shouldUseTemporalHistory) ||
        (dxrReflectionRenderer_->GetSettings().isEnabled && dxrReflectionRenderer_->GetSettings().shouldUseTemporalHistory) ||
        (dxrGlobalIlluminationRenderer_->GetSettings().isEnabled && dxrGlobalIlluminationRenderer_->GetSettings().shouldUseTemporalHistory) ||
        (dxrLocalShadowRenderer_->GetSettings().isEnabled && dxrLocalShadowRenderer_->GetSettings().shouldUseTemporalHistory) ||
        (dxrShadowRenderer_->GetSettings().isEnabled && dxrShadowRenderer_->GetSettings().isDenoisingEnabled
            && dxrShadowRenderer_->GetSettings().shouldUseTemporalHistory)) {
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
        defaultCamera, volumetricShadows, volumetricLocalShadows, sceneManager->GetSceneRevision());

    // Offscreen draw start
    dxrRenderer_->BeginFrame();
    dxrLocalShadowRenderer_->Prepare(*dxrRenderer_);
    postEffectManager_->PreDrawDepth();
    offscreenRenderer_->SetClearColor(sceneManager->GetSceneClearColor());
    bool shouldCaptureDirectionalLight = dxrRenderer_->IsReady() && dxrRenderer_->GetSettings().isEnabled
        && dxrShadowRenderer_->GetSettings().isEnabled;
    bool shouldCaptureReflections = dxrRenderer_->IsReady() && dxrRenderer_->GetSettings().isEnabled
        && (dxrReflectionRenderer_->GetSettings().isEnabled || dxrGlobalIlluminationRenderer_->GetSettings().isEnabled);
    bool shouldCaptureLocalShadows = dxrLocalShadowRenderer_->GetSelectedLightCount() > 0;
    offscreenRenderer_->PreDraw(postEffectManager_->GetDepthDSVHandle(), shouldCaptureDirectionalLight, shouldCaptureReflections, shouldCaptureLocalShadows);
    dxrRenderer_->SetDirectionalShadowCapture(offscreenRenderer_->IsDirectionalCaptureActive());
    dxrRenderer_->SetReflectionCapture(offscreenRenderer_->IsReflectionCaptureActive());
    dxrRenderer_->SetLocalShadowCapture(offscreenRenderer_->IsLocalShadowCaptureActive());
    if (!offscreenRenderer_->IsLocalShadowCaptureActive()) { dxrRenderer_->SetLocalShadowParameters({}, 0); }
    if (!isLowResolutionRenderingEnabled_) { dlssSuperResolution_->SetSceneViewport(); }
    if (motionSceneRevision_ != sceneManager->GetSceneRevision()) {
        motionVectorRenderer_->ResetHistory();
        motionSceneRevision_ = sceneManager->GetSceneRevision();
    }
    motionVectorRenderer_->BeginFrame();
    if (DxrRenderer::GetActive() == dxrRenderer_.get()) {
        bool hasExplicitScene = sceneManager->SubmitRaytracingScene(*dxrRenderer_);
        dxrRenderer_->SetDrawSubmissionEnabled(!hasExplicitScene);
    }
    sceneManager->Draw3D();
    dxrRenderer_->EndFrame(defaultCamera, dxrRenderer_->GetSettings().isDebugVisible);
    DebugRenderer::GetInstance()->Draw();
    motionVectorRenderer_->EndFrame(postEffectManager_->GetDepthDSVHandle());
    motionVectorRenderer_->SetSettings(motionSettings);
    postEffectManager_->PostDrawDepth();
    offscreenRenderer_->PostDraw();
    DxrShadowInputs rtShadowInputs;
    rtShadowInputs.scene = dxrRenderer_.get();
    rtShadowInputs.camera = defaultCamera;
    rtShadowInputs.depthTexture = postEffectManager_->GetDepthTexture();
    rtShadowInputs.normalTexture = offscreenRenderer_->GetNormalTexture();
    rtShadowInputs.directionalLightTexture = offscreenRenderer_->GetDirectionalLightTexture();
    rtShadowInputs.colorSrv = offscreenRenderer_->GetSrvHandleGPU();
    rtShadowInputs.depthSrv = postEffectManager_->GetDepthSrv();
    rtShadowInputs.normalSrv = offscreenRenderer_->GetNormalSrvHandleGPU();
    rtShadowInputs.directionalLightSrv = offscreenRenderer_->GetDirectionalLightSrv();
    rtShadowInputs.lightDirection = lights->GetDirectionalDirection();
    rtShadowInputs.motionVectorSrv = motionVectorRenderer_->GetSrvHandle(); rtShadowInputs.reprojectionSrv = motionVectorRenderer_->GetReprojectionSrv(); rtShadowInputs.previousReprojectionSrv = motionVectorRenderer_->GetPreviousReprojectionSrv();
    rtShadowInputs.sceneRevision = sceneManager->GetSceneRevision();
    D3D12_GPU_DESCRIPTOR_HANDLE sceneWithRtShadows = dxrShadowRenderer_->Draw(rtShadowInputs);
    DxrLocalShadowInputs localShadowInputs;
    localShadowInputs.scene = dxrRenderer_.get(); localShadowInputs.camera = defaultCamera;
    if (lights->GetLightingComponents().w > 0.5f) { localShadowInputs.camera = nullptr; }
    localShadowInputs.colorSrv = sceneWithRtShadows;
    localShadowInputs.depthTexture = postEffectManager_->GetDepthTexture(); localShadowInputs.depthSrv = postEffectManager_->GetDepthSrv();
    localShadowInputs.surfaceTexture = offscreenRenderer_->GetReflectionSurfaceTexture(); localShadowInputs.surfaceSrv = offscreenRenderer_->GetReflectionSurfaceSrv();
    localShadowInputs.environmentTexture = offscreenRenderer_->GetReflectionEnvironmentTexture(); localShadowInputs.environmentSrv = offscreenRenderer_->GetReflectionEnvironmentSrv();
    localShadowInputs.materialTexture = offscreenRenderer_->GetMaterialTexture(); localShadowInputs.materialSrv = offscreenRenderer_->GetMaterialSrvHandleGPU();
    localShadowInputs.localLightTexture = offscreenRenderer_->GetLocalLightTexture(); localShadowInputs.localLightSrv = offscreenRenderer_->GetLocalLightSrv();
    localShadowInputs.motionVectorSrv = motionVectorRenderer_->GetSrvHandle(); localShadowInputs.reprojectionSrv = motionVectorRenderer_->GetReprojectionSrv(); localShadowInputs.previousReprojectionSrv = motionVectorRenderer_->GetPreviousReprojectionSrv(); localShadowInputs.sceneRevision = sceneManager->GetSceneRevision();
    auto sceneWithLocalShadows = dxrLocalShadowRenderer_->Draw(localShadowInputs);
    bool isRtLocalShadowActive = dxrLocalShadowRenderer_->HasValidFrame();
    if (!isRtLocalShadowActive) { dxrRenderer_->SetLocalShadowParameters({}, 0); }
    if (wasRtLocalShadowActive_ != isRtLocalShadowActive) {
        screenSpaceGlobalIllumination_->ResetHistory(); screenSpaceReflection_->ResetHistory(); dlssSuperResolution_->ResetHistory(); temporalSuperResolution_->ResetHistory();
        dxrGlobalIlluminationRenderer_->ResetHistory(); dxrReflectionRenderer_->ResetHistory();
        wasRtLocalShadowActive_ = isRtLocalShadowActive;
    }
    bool isLocalShadowDebugView = isRtLocalShadowActive && dxrLocalShadowRenderer_->GetSettings().isDebugVisible;
    DirectXCommon::GetInstance()->PreDraw();
    ScreenSpaceGlobalIlluminationInputs globalIlluminationInputs;
    globalIlluminationInputs.colorSrv = sceneWithLocalShadows;
    globalIlluminationInputs.depthSrv = postEffectManager_->GetDepthSrv();
    globalIlluminationInputs.normalSrv = offscreenRenderer_->GetNormalSrvHandleGPU();
    globalIlluminationInputs.materialSrv = offscreenRenderer_->GetMaterialSrvHandleGPU();
    globalIlluminationInputs.motionVectorSrv = motionVectorRenderer_->GetSrvHandle();
    globalIlluminationInputs.camera = defaultCamera;
    globalIlluminationInputs.sceneRevision = sceneManager->GetSceneRevision();
    bool isLightingComponentView = lights->GetLightingComponents().w > 0.5f || isLocalShadowDebugView;
    if (isLightingComponentView) { globalIlluminationInputs.camera = nullptr; }
    DxrGlobalIlluminationInputs rtGlobalIlluminationInputs;
    rtGlobalIlluminationInputs.scene = dxrRenderer_.get(); rtGlobalIlluminationInputs.camera = globalIlluminationInputs.camera;
    rtGlobalIlluminationInputs.colorSrv = sceneWithLocalShadows;
    rtGlobalIlluminationInputs.depthTexture = postEffectManager_->GetDepthTexture(); rtGlobalIlluminationInputs.depthSrv = postEffectManager_->GetDepthSrv();
    rtGlobalIlluminationInputs.surfaceTexture = offscreenRenderer_->GetReflectionSurfaceTexture(); rtGlobalIlluminationInputs.surfaceSrv = offscreenRenderer_->GetReflectionSurfaceSrv();
    rtGlobalIlluminationInputs.environmentTexture = offscreenRenderer_->GetReflectionEnvironmentTexture(); rtGlobalIlluminationInputs.environmentSrv = offscreenRenderer_->GetReflectionEnvironmentSrv();
    rtGlobalIlluminationInputs.materialTexture = offscreenRenderer_->GetMaterialTexture(); rtGlobalIlluminationInputs.materialSrv = offscreenRenderer_->GetMaterialSrvHandleGPU();
    rtGlobalIlluminationInputs.indirectTexture = offscreenRenderer_->GetIndirectTexture(); rtGlobalIlluminationInputs.indirectSrv = offscreenRenderer_->GetIndirectSrvHandleGPU();
    rtGlobalIlluminationInputs.motionVectorSrv = motionVectorRenderer_->GetSrvHandle(); rtGlobalIlluminationInputs.reprojectionSrv = motionVectorRenderer_->GetReprojectionSrv(); rtGlobalIlluminationInputs.previousReprojectionSrv = motionVectorRenderer_->GetPreviousReprojectionSrv(); rtGlobalIlluminationInputs.sceneRevision = sceneManager->GetSceneRevision();
    auto rtIndirectColor = dxrGlobalIlluminationRenderer_->Draw(rtGlobalIlluminationInputs);
    bool isRtGlobalIlluminationActive = dxrGlobalIlluminationRenderer_->HasValidFrame();
    if (wasRtGlobalIlluminationActive_ != isRtGlobalIlluminationActive) {
        screenSpaceGlobalIllumination_->ResetHistory(); screenSpaceReflection_->ResetHistory();
        dxrReflectionRenderer_->ResetHistory(); dlssSuperResolution_->ResetHistory(); temporalSuperResolution_->ResetHistory();
        wasRtGlobalIlluminationActive_ = isRtGlobalIlluminationActive;
    }
    if (isRtGlobalIlluminationActive) { globalIlluminationInputs.camera = nullptr; }
    if (screenSpaceGlobalIllumination_->IsEnabled() &&
        screenSpaceGlobalIllumination_->GetSettings().shouldUseHierarchicalDepth && !isLightingComponentView && !isRtGlobalIlluminationActive) {
        ScreenSpaceReflectionInputs depthInputs;
        depthInputs.colorSrv = globalIlluminationInputs.colorSrv;
        depthInputs.depthSrv = globalIlluminationInputs.depthSrv;
        depthInputs.normalSrv = globalIlluminationInputs.normalSrv;
        depthInputs.camera = defaultCamera;
        screenSpaceReflection_->PrepareDepthPyramid(depthInputs);
        globalIlluminationInputs.depthPyramidSrv = screenSpaceReflection_->GetDepthPyramidSrv();
    }
    D3D12_GPU_DESCRIPTOR_HANDLE sceneWithIndirectLight = screenSpaceGlobalIllumination_->Draw(globalIlluminationInputs);
    if (isRtGlobalIlluminationActive) { sceneWithIndirectLight = rtIndirectColor; }
    bool isGlobalIlluminationDebugView = !isRtGlobalIlluminationActive && screenSpaceGlobalIllumination_->IsEnabled() &&
        screenSpaceGlobalIllumination_->GetSettings().debugMode != ScreenSpaceGlobalIlluminationDebugMode::None;
    if (isRtGlobalIlluminationActive && dxrGlobalIlluminationRenderer_->GetSettings().isDebugVisible) { isGlobalIlluminationDebugView = true; }
    if (isLocalShadowDebugView) { isGlobalIlluminationDebugView = true; }
    postEffectManager_->SetIndirectLightingDebugVisible(isGlobalIlluminationDebugView);
    DxrReflectionInputs rtReflectionInputs;
    rtReflectionInputs.scene = dxrRenderer_.get(); rtReflectionInputs.camera = defaultCamera;
    rtReflectionInputs.colorSrv = sceneWithIndirectLight;
    rtReflectionInputs.depthTexture = postEffectManager_->GetDepthTexture(); rtReflectionInputs.depthSrv = postEffectManager_->GetDepthSrv();
    rtReflectionInputs.surfaceTexture = offscreenRenderer_->GetReflectionSurfaceTexture(); rtReflectionInputs.surfaceSrv = offscreenRenderer_->GetReflectionSurfaceSrv();
    rtReflectionInputs.environmentTexture = offscreenRenderer_->GetReflectionEnvironmentTexture(); rtReflectionInputs.environmentSrv = offscreenRenderer_->GetReflectionEnvironmentSrv();
    rtReflectionInputs.materialTexture = offscreenRenderer_->GetMaterialTexture(); rtReflectionInputs.materialSrv = offscreenRenderer_->GetMaterialSrvHandleGPU();
    rtReflectionInputs.motionVectorSrv = motionVectorRenderer_->GetSrvHandle(); rtReflectionInputs.reprojectionSrv = motionVectorRenderer_->GetReprojectionSrv(); rtReflectionInputs.previousReprojectionSrv = motionVectorRenderer_->GetPreviousReprojectionSrv(); rtReflectionInputs.sceneRevision = sceneManager->GetSceneRevision();
    if (isLightingComponentView || isGlobalIlluminationDebugView) { rtReflectionInputs.camera = nullptr; }
    auto sceneWithRtReflections = dxrReflectionRenderer_->Draw(rtReflectionInputs);
    postEffectManager_->PrepareSceneForTemporalResolve(sceneManager, sceneWithRtReflections);

    ScreenSpaceReflectionInputs reflectionInputs;
    reflectionInputs.colorSrv = postEffectManager_->GetSceneColorSrv();
    reflectionInputs.depthSrv = postEffectManager_->GetDepthSrv();
    reflectionInputs.normalSrv = offscreenRenderer_->GetNormalSrvHandleGPU();
    reflectionInputs.motionVectorSrv = motionVectorRenderer_->GetSrvHandle();
    reflectionInputs.sceneRevision = sceneManager->GetSceneRevision();
    reflectionInputs.camera = defaultCamera;
    // Reflection is a separate lighting contribution; isolate the selected source view.
    if (isLightingComponentView || isGlobalIlluminationDebugView || dxrReflectionRenderer_->HasValidFrame()) { reflectionInputs.camera = nullptr; }
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
    if (temporalSuperResolution_->IsActive()) {
        TemporalResolutionFrameInputs temporalInputs; temporalInputs.scene = frameInputs;
        temporalInputs.camera = defaultCamera; temporalInputs.depthSrv = postEffectManager_->GetDepthSrv();
        temporalInputs.motionSrv = motionVectorRenderer_->GetSrvHandle();
        temporalInputs.reprojectionSrv = motionVectorRenderer_->GetReprojectionSrv();
        temporalInputs.reprojectionTexture = motionVectorRenderer_->GetReprojectionTexture();
        sceneColorHandle = temporalSuperResolution_->Evaluate(temporalInputs);
    }
    postEffectManager_->ResolveSceneColor(sceneColorHandle);
    postEffectManager_->SetParticleDepthJitter({projectionJitter.x * 0.5f, projectionJitter.y * -0.5f});
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
    dxrRenderer_->DrawDebug();

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
    dxrRenderer_->ReadCompleted();
    dxrReflectionRenderer_->ReadCompleted();
    dxrGlobalIlluminationRenderer_->ReadCompleted();
    dxrLocalShadowRenderer_->ReadCompleted();
    dxrShadowRenderer_->ReadCompleted();
    postEffectManager_->ReadCompletedGpuTiming();
    postEffectManager_->GetVolumetricLightRenderer()->ReadCompleted();
    temporalSuperResolution_->ReadCompleted();
    RecordGpuSample();
    ScreenshotManager::GetInstance()->CompleteCapture();
}

void Renderer::SetAntiAliasing(bool isDlaaEnabled, bool isFxaaEnabled)
{
    if (!temporalSuperResolution_->GetSettings().isEnabled && dlssSuperResolution_->IsEnabled() == isDlaaEnabled
        && postEffectManager_->IsFxaaEnabled() == isFxaaEnabled) { return; }
    auto temporalSettings = temporalSuperResolution_->GetSettings(); temporalSettings.isEnabled = false;
    temporalSuperResolution_->SetSettings(temporalSettings);
    dlssSuperResolution_->SetEnabled(isDlaaEnabled);
    postEffectManager_->SetFxaaEnabled(isFxaaEnabled);
    dlssSuperResolution_->ResetHistory(); temporalSuperResolution_->ResetHistory();
    motionVectorRenderer_->ResetHistory();
    screenSpaceReflection_->ResetHistory();
    screenSpaceGlobalIllumination_->ResetHistory();
    warmupFrames_ = 32;
    previousAntiAliasingMode_ = UINT_MAX;
}

void Renderer::SetTemporalAntiAliasing(bool isEnabled, bool isFxaaEnabled)
{
    SetAntiAliasing(false, isFxaaEnabled);
    auto settings = temporalSuperResolution_->GetSettings(); settings.isEnabled = isEnabled;
    temporalSuperResolution_->SetSettings(settings);
    temporalSuperResolution_->ResetHistory(); motionVectorRenderer_->ResetHistory();
    warmupFrames_ = 32; previousAntiAliasingMode_ = UINT_MAX;
}

uint32_t Renderer::GetAntiAliasingMode() const
{
    if (temporalSuperResolution_->GetSettings().isEnabled) {
        if (postEffectManager_->IsFxaaEnabled()) { return 5; }
        return 4;
    }
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
    if (mode < 4 && dlssSuperResolution_->IsEnabled() && !dlssSuperResolution_->IsActive()) { return; }
    if (mode >= 4 && !temporalSuperResolution_->IsActive()) { return; }
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
    const char* kModeNames[] = {"OFF", "DLAA", "FXAA", "DLAA + FXAA", "TAA", "TAA + FXAA"};
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
    return {{"qualityPreset", static_cast<uint32_t>(GetRenderQualityPreset())}, {"lowResolutionEnabled", isLowResolutionRenderingEnabled_}, {"sceneRenderWidth", sceneRenderWidth_}, {"sceneRenderHeight", sceneRenderHeight_},
        {"taaEnabled", temporalSuperResolution_->GetSettings().isEnabled},
        {"taaGpuMs", temporalSuperResolution_->GetGpuTimeMs()}, {"taaAllocationBytes", temporalSuperResolution_->GetAllocationBytes()},
        {"dlaaEnabled", dlssSuperResolution_->IsEnabled()}, {"fxaaEnabled", postEffectManager_->IsFxaaEnabled()},
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
        {{"key", "qualityHigh"}, {"label", "画質優先"}, {"type", "action"}},
        {{"key", "qualityBalanced"}, {"label", "バランス"}, {"type", "action"}},
        {{"key", "qualityPerformance"}, {"label", "速度優先"}, {"type", "action"}},
        {{"key", "qualityPreset"}, {"label", "品質 (0:画質 / 1:均衡 / 2:速度 / 3:個別)"}, {"type", "metric"}},
        {{"key", "qualityResolution"}, {"label", "推奨解像度を適用"}, {"type", "action"}},
        {{"key", "lowResolutionEnabled"}, {"label", "低解像度描画"}, {"type", "bool"}},
        {{"key", "resolution75"}, {"label", "960 × 540"}, {"type", "action"}},
        {{"key", "resolution50"}, {"label", "640 × 360"}, {"type", "action"}},
        {{"key", "sceneRenderWidth"}, {"label", "3D描画の幅"}, {"type", "metric"}},
        {{"key", "sceneRenderHeight"}, {"label", "3D描画の高さ"}, {"type", "metric"}},
        {{"key", "aaOff"}, {"label", "AAなし"}, {"type", "action"}},
        {{"key", "aaFxaa"}, {"label", "FXAAのみ"}, {"type", "action"}},
        {{"key", "aaDlaa"}, {"label", "DLAAのみ"}, {"type", "action"}},
        {{"key", "aaTaa"}, {"label", "汎用TAA"}, {"type", "action"}},
        {{"key", "aaTaaFxaa"}, {"label", "TAA + FXAA"}, {"type", "action"}},
        {{"key", "taaGpuMs"}, {"label", "TAA / 拡大 GPU (ms)"}, {"type", "metric"}},
        {{"key", "taaAllocationBytes"}, {"label", "TAA画像 bytes"}, {"type", "metric"}},
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
    if (key == "lowResolutionEnabled") { return SetLowResolutionRendering(isEnabled, lowResolutionWidth_, lowResolutionHeight_); }
    if (key == "ssrEnabled") { screenSpaceReflection_->SetEnabled(isEnabled); return true; }
    if (key == "dlaaEnabled") { SetAntiAliasing(isEnabled, postEffectManager_->IsFxaaEnabled()); return true; }
    if (key == "fxaaEnabled") {
        if (temporalSuperResolution_->GetSettings().isEnabled) { SetTemporalAntiAliasing(true, isEnabled); return true; }
        SetAntiAliasing(dlssSuperResolution_->IsEnabled(), isEnabled); return true;
    }
    return false;
}

bool Renderer::ExecuteDevelopmentCommand(const std::string& key)
{
    if (key == "qualityHigh") { return SetRenderQualityPreset(RenderQualityPreset::Quality); }
    if (key == "qualityBalanced") { return SetRenderQualityPreset(RenderQualityPreset::Balanced); }
    if (key == "qualityPerformance") { return SetRenderQualityPreset(RenderQualityPreset::Performance); }
    if (key == "qualityResolution") { return ApplyRecommendedRenderResolution(); }
    if (key == "resolution75") { return SetLowResolutionRendering(true, 960, 540); }
    if (key == "resolution50") { return SetLowResolutionRendering(true, 640, 360); }
    if (key == "aaTaa") { SetTemporalAntiAliasing(true, false); return true; }
    if (key == "aaTaaFxaa") { SetTemporalAntiAliasing(true, true); return true; }
    if (key == "aaOff") { SetAntiAliasing(false, false); return true; }
    if (key == "aaFxaa") { SetAntiAliasing(false, true); return true; }
    if (key == "aaDlaa") { SetAntiAliasing(true, false); return true; }
    if (key == "aaBoth") { SetAntiAliasing(true, true); return true; }
    return false;
}
#endif

bool Renderer::SetLowResolutionRendering(bool isEnabled, uint32_t width, uint32_t height) {
    if (width < 320 || height < 180 || width > WinApp::kClientWidth || height > WinApp::kClientHeight
        || width % 2 != 0 || height % 2 != 0 || uint64_t(width) * WinApp::kClientHeight != uint64_t(height) * WinApp::kClientWidth) { return false; }
    isLowResolutionRenderingEnabled_ = isEnabled; lowResolutionWidth_ = width; lowResolutionHeight_ = height;
    return true;
}
void Renderer::ApplySceneRenderResolution() {
    uint32_t width = WinApp::kClientWidth; uint32_t height = WinApp::kClientHeight;
    if (isLowResolutionRenderingEnabled_) { width = lowResolutionWidth_; height = lowResolutionHeight_; }
    if (width == sceneRenderWidth_ && height == sceneRenderHeight_) { return; }
    // Previous Draw's PostDraw waited for the fence. No commands reference these targets yet.
    sceneRenderWidth_ = width; sceneRenderHeight_ = height;
    SceneRenderResolution::SetSize(width, height);
    offscreenRenderer_->ResizeSceneTargets(); motionVectorRenderer_->ResizeSceneTargets();
    screenSpaceReflection_->ResizeSceneTargets(); screenSpaceGlobalIllumination_->ResizeSceneTargets();
    postEffectManager_->ResizeSceneTargets();
    auto settings = temporalSuperResolution_->GetSettings(); settings.inputWidth = width; settings.inputHeight = height;
    temporalSuperResolution_->SetSettings(settings); dlssSuperResolution_->ResetHistory();
    dxrShadowRenderer_->ResetHistory(); dxrReflectionRenderer_->ResetHistory(); dxrGlobalIlluminationRenderer_->ResetHistory(); dxrLocalShadowRenderer_->ResetHistory();
    postEffectManager_->GetAutoExposureRenderer()->ResetHistory();
    antiAliasingSamples_ = {}; warmupFrames_ = 32;
}

namespace {
struct RenderQualityProfile {
    uint32_t sunSamples;
    uint32_t lightingSamples;
    uint32_t spatialPasses;
    uint32_t shadowHistoryFrames;
    uint32_t reflectionBounces;
    VolumetricQuality volumeQuality;
    int32_t volumeSteps;
};
constexpr RenderQualityProfile kRenderQualityProfiles[] = {
    {16, 8, 3, 32, 2, VolumetricQuality::High, 64},
    {8, 4, 2, 32, 1, VolumetricQuality::Medium, 32},
    {2, 1, 1, 16, 1, VolumetricQuality::Low, 16}
};
}
bool Renderer::SetRenderQualityPreset(RenderQualityPreset preset) {
    uint32_t profileIndex = static_cast<uint32_t>(preset);
    if (profileIndex >= _countof(kRenderQualityProfiles)) { return false; }
    const auto& profile = kRenderQualityProfiles[profileIndex];
    auto shadow = dxrShadowRenderer_->GetSettings();
    shadow.sampleCount = profile.sunSamples; shadow.spatialPassCount = profile.spatialPasses;
    shadow.maxHistoryFrames = profile.shadowHistoryFrames; shadow.isDenoisingEnabled = true; shadow.shouldUseTemporalHistory = true;
    auto reflection = dxrReflectionRenderer_->GetSettings();
    reflection.sampleCount = profile.lightingSamples; reflection.spatialPassCount = profile.spatialPasses;
    reflection.maxReflectionBounces = profile.reflectionBounces;
    reflection.shouldTraceMultipleReflections = profile.reflectionBounces > 1;
    reflection.shouldUseTemporalHistory = true;
    auto indirect = dxrGlobalIlluminationRenderer_->GetSettings();
    indirect.sampleCount = profile.lightingSamples; indirect.spatialPassCount = profile.spatialPasses; indirect.shouldUseTemporalHistory = true;
    auto local = dxrLocalShadowRenderer_->GetSettings();
    local.sampleCount = profile.lightingSamples; local.spatialPassCount = profile.spatialPasses; local.shouldUseTemporalHistory = true;
    if (!dxrShadowRenderer_->SetSettings(shadow) || !dxrReflectionRenderer_->SetSettings(reflection)
        || !dxrGlobalIlluminationRenderer_->SetSettings(indirect) || !dxrLocalShadowRenderer_->SetSettings(local)) { return false; }
    auto* volume = postEffectManager_->GetVolumetricLightRenderer();
    volume->SetQuality(profile.volumeQuality); volume->SetTemporalEnabled(true);
    // Quality changes alter HDR radiance and history filtering even with unchanged dimensions.
    dxrShadowRenderer_->ResetHistory(); dxrReflectionRenderer_->ResetHistory();
    dxrGlobalIlluminationRenderer_->ResetHistory(); dxrLocalShadowRenderer_->ResetHistory(); volume->ResetHistory();
    temporalSuperResolution_->ResetHistory(); dlssSuperResolution_->ResetHistory();
    screenSpaceReflection_->ResetHistory(); screenSpaceGlobalIllumination_->ResetHistory();
    postEffectManager_->GetAutoExposureRenderer()->ResetHistory();
    antiAliasingSamples_ = {}; warmupFrames_ = 32;
    return true;
}
RenderQualityPreset Renderer::GetRenderQualityPreset() const {
    const auto& shadow = dxrShadowRenderer_->GetSettings(); const auto& reflection = dxrReflectionRenderer_->GetSettings();
    const auto& indirect = dxrGlobalIlluminationRenderer_->GetSettings(); const auto& local = dxrLocalShadowRenderer_->GetSettings();
    const auto& volume = postEffectManager_->GetVolumetricLightRenderer()->GetParameters();
    for (uint32_t index = 0; index < _countof(kRenderQualityProfiles); ++index) {
        const auto& profile = kRenderQualityProfiles[index];
        bool shouldTraceMultipleReflections = profile.reflectionBounces > 1;
        if (shadow.sampleCount == profile.sunSamples && shadow.spatialPassCount == profile.spatialPasses
            && shadow.maxHistoryFrames == profile.shadowHistoryFrames && shadow.isDenoisingEnabled && shadow.shouldUseTemporalHistory
            && reflection.sampleCount == profile.lightingSamples && reflection.spatialPassCount == profile.spatialPasses
            && reflection.maxReflectionBounces == profile.reflectionBounces && reflection.shouldTraceMultipleReflections == shouldTraceMultipleReflections
            && reflection.shouldUseTemporalHistory && indirect.sampleCount == profile.lightingSamples
            && indirect.spatialPassCount == profile.spatialPasses && indirect.shouldUseTemporalHistory
            && local.sampleCount == profile.lightingSamples && local.spatialPassCount == profile.spatialPasses && local.shouldUseTemporalHistory
            && volume.sampleCount == profile.volumeSteps && volume.shouldUseTemporalHistory) { return static_cast<RenderQualityPreset>(index); }
    }
    return RenderQualityPreset::Custom;
}
bool Renderer::ApplyRecommendedRenderResolution() {
    auto preset = GetRenderQualityPreset();
    if (preset == RenderQualityPreset::Custom) { return false; }
    if (preset == RenderQualityPreset::Performance) {
        SetLowResolutionRendering(true, 960, 540); SetTemporalAntiAliasing(true, false); return true;
    }
    return SetLowResolutionRendering(false, lowResolutionWidth_, lowResolutionHeight_);
}

uint64_t Renderer::UpdateTemporalRadianceRevision() {
    auto* lights = LightManager::GetInstance(); const auto sun = lights->GetDirectionalLight();
    const Vector4 kLightingValues[] = {sun.color, lights->GetEnvironmentLighting(), lights->GetAtmosphereSettings(),
        lights->GetHemisphereSkyColor(), lights->GetHemisphereGroundColor()};
    std::array<float, 21> values = {}; values[20] = sun.intensity;
    for (uint32_t index = 0; index < _countof(kLightingValues); ++index) {
        const auto& value = kLightingValues[index];
        values[index * 4] = value.x; values[index * 4 + 1] = value.y;
        values[index * 4 + 2] = value.z; values[index * 4 + 3] = value.w;
    }
    bool hasAbruptChange = !hasTemporalRadiance_;
    constexpr float kRelativeChangeThreshold = 0.1f;
    constexpr float kAbsoluteChangeFloor = 0.00001f;
    for (uint32_t index = 0; index < values.size(); ++index) {
        float previous = previousTemporalRadiance_[index]; float current = values[index];
        float scale = std::abs(previous); if (std::abs(current) > scale) { scale = std::abs(current); }
        float threshold = scale * kRelativeChangeThreshold; if (threshold < kAbsoluteChangeFloor) { threshold = kAbsoluteChangeFloor; }
        if (!std::isfinite(previous) || !std::isfinite(current) || std::abs(current - previous) > threshold) { hasAbruptChange = true; }
    }
    const auto& previousDirection = previousTemporalSunDirection_;
    float previousLengthSquared = previousDirection[0] * previousDirection[0] + previousDirection[1] * previousDirection[1] + previousDirection[2] * previousDirection[2];
    float currentLengthSquared = sun.direction.x * sun.direction.x + sun.direction.y * sun.direction.y + sun.direction.z * sun.direction.z;
    constexpr float kSunDirectionCosThreshold = 0.9986295f; // Three degrees per frame.
    if (!std::isfinite(previousLengthSquared) || !std::isfinite(currentLengthSquared) || previousLengthSquared < 0.000001f || currentLengthSquared < 0.000001f) {
        hasAbruptChange = true;
    } else {
        float alignment = (previousDirection[0] * sun.direction.x + previousDirection[1] * sun.direction.y + previousDirection[2] * sun.direction.z)
            / std::sqrt(previousLengthSquared * currentLengthSquared);
        if (!std::isfinite(alignment) || alignment < kSunDirectionCosThreshold) { hasAbruptChange = true; }
    }
    if (hasAbruptChange) { ++temporalRadianceRevision_; }
    // Compare adjacent frames so a gradual day/night transition keeps accumulating.
    // Per-pixel reactive weighting still rejects abrupt local color changes.
    previousTemporalRadiance_ = values;
    previousTemporalSunDirection_ = {sun.direction.x, sun.direction.y, sun.direction.z}; hasTemporalRadiance_ = true;
    return temporalRadianceRevision_;
}

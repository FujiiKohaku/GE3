#include "Engine/PostEffect/PostEffectManager.h"

#include "App/Scene/SceneManager.h"
#include "Engine/Camera/Camera.h"
#include "Engine/PostEffect/Bloom/BloomRenderer.h"
#include "Engine/PostEffect/Fog/FogManager.h"
#include "Engine/PostEffect/Fog/FogRenderer.h"
#include "Engine/SrvManager/SrvManager.h"
#include "Engine/Winapp/WinApp.h"
#include <cassert>
#include <cstddef>
#include <cstdio>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

PostEffectManager::PostEffectManager() = default;

PostEffectManager::~PostEffectManager() = default;

void PostEffectManager::Initialize(DirectXCommon* dxCommon)
{
    assert(dxCommon != nullptr);
    dxCommon_ = dxCommon;

    copyImageRenderer_ = std::make_unique<CopyImageRenderer>();
    copyImageRenderer_->Initialize(dxCommon_);

    bloomRenderer_ = std::make_unique<BloomRenderer>();
    bloomRenderer_->Initialize(dxCommon_);

    fogManager_ = std::make_unique<FogManager>();
    fogManager_->Initialize(dxCommon_);

    fogRenderer_ = std::make_unique<FogRenderer>();
    fogRenderer_->Initialize(dxCommon_);

    for (uint32_t index = 0; index < kPingPongRenderTargetCount; ++index) {
        pingPongRenderTargets_[index].Initialize(dxCommon_, kFirstPingPongRTVIndex + index);
    }
}

void PostEffectManager::Update(Camera* camera)
{
    if (camera != nullptr) {
        fogManager_->SetCameraInfo(
            camera->GetNearClip(),
            camera->GetFarClip(),
            camera->GetFovY(),
            camera->GetAspectRatio());
    }

    fogManager_->Update();
    bloomRenderer_->Update();
}

void PostEffectManager::DrawImGui()
{
    fogManager_->DrawImGui();
    bloomRenderer_->DrawImGui();

#ifdef USE_IMGUI
    ImGui::Begin("Post Effects");

    CopyImageRenderer::PostEffectParameter& parameter = copyImageRenderer_->GetPostEffectParameter();
    const char* animationButtonLabel = "Animated Effects: OFF";
    if (isAnimationEnabled_) {
        animationButtonLabel = "Animated Effects: ON";
    }
    if (ImGui::Button(animationButtonLabel)) {
        isAnimationEnabled_ = !isAnimationEnabled_;
        if (isAnimationEnabled_) { parameter.animationEnabled = 1; } else { parameter.animationEnabled = 0; }
    }
    ImGui::SliderFloat("Pixel Size", &parameter.pixelSize, 1.0f, 64.0f, "%.0f");
    ImGui::SliderFloat("Brightness", &parameter.colorBrightness, -1.0f, 1.0f);
    ImGui::SliderFloat("Contrast", &parameter.colorContrast, 0.0f, 3.0f);
    ImGui::SliderFloat("Saturation", &parameter.colorSaturation, 0.0f, 3.0f);
    ImGui::SliderFloat("Focus Depth", &parameter.focusDepth, 0.0f, 1.0f, "%.4f");
    ImGui::SliderFloat("Focus Range", &parameter.focusRange, 0.0001f, 0.2f, "%.4f");
    ImGui::SliderFloat("DoF Radius", &parameter.depthOfFieldRadius, 0.0f, 32.0f);
    ImGui::SliderFloat2("Motion Direction", &parameter.motionBlurDirection.x, -1.0f, 1.0f);
    ImGui::SliderFloat("Motion Strength", &parameter.motionBlurStrength, 0.0f, 0.1f);
    ImGui::SliderInt("Motion Samples", &parameter.motionBlurSampleCount, 1, 32);
    ImGui::SliderFloat("Chromatic Aberration", &parameter.chromaticAberrationStrength, 0.0f, 0.05f);
    ImGui::SliderFloat("Lens Distortion", &parameter.lensDistortionStrength, -1.0f, 1.0f);
    ImGui::SliderFloat("Film Grain", &parameter.filmGrainStrength, 0.0f, 0.5f);
    ImGui::SliderFloat("Lens Dirt", &parameter.lensDirtStrength, 0.0f, 3.0f);
    float cameraShakeStrength = SceneManager::GetInstance()->GetCameraShakeStrength();
    if (ImGui::SliderFloat("Camera Shake", &cameraShakeStrength, 0.0f, 0.05f)) {
        SceneManager::GetInstance()->SetCameraShakeStrength(cameraShakeStrength);
    }
    ImGui::SliderFloat("Bokeh Radius", &parameter.bokehRadius, 0.0f, 32.0f);
    ImGui::SliderInt("Bokeh Sides", &parameter.bokehSides, 3, 12);
    ImGui::SliderFloat("Fisheye", &parameter.fisheyeStrength, 0.01f, 3.0f);
    ImGui::SliderFloat("Light Threshold", &parameter.lightThreshold, 0.0f, 2.0f);
    ImGui::SliderFloat("Light Strength", &parameter.lightStrength, 0.0f, 5.0f);
    ImGui::SliderFloat("Light Radius", &parameter.lightRadius, 0.01f, 1.0f);
    ImGui::Separator();

    const std::vector<PostEffectInfo>& postEffects = SceneManager::GetInstance()->GetPostEffects();
    if (postEffects.empty()) {
        ImGui::Text("1: Copy (implicit)");
    }

    for (std::size_t index = 0; index < postEffects.size(); ++index) {
        const PostEffectInfo& postEffect = postEffects[index];

        ImGui::Text("%u: %s", static_cast<unsigned int>(index + 1), GetPostEffectTypeName(postEffect.type));
        ImGui::SameLine();

        bool enabled = postEffect.enabled;
        char label[64] {};
        sprintf_s(label, sizeof(label), "Enabled##PostEffect%u", static_cast<unsigned int>(index));
        if (ImGui::Checkbox(label, &enabled)) {
            SceneManager::GetInstance()->SetPostEffectEnabled(postEffect.type, enabled);
        }
    }

    ImGui::End();
#endif
}

void PostEffectManager::PreDrawDepth()
{
    fogRenderer_->PreDrawDepth();
}

void PostEffectManager::PostDrawDepth()
{
    fogRenderer_->PostDrawDepth();
}

void PostEffectManager::PrepareDepthForParticleDraw()
{
    fogRenderer_->PrepareDepthForParticleDraw();
}

void PostEffectManager::SetBoostRadialBlurParameters(bool isBoosting)
{
    CopyImageRenderer::PostEffectParameter& postEffectParameter = copyImageRenderer_->GetPostEffectParameter();
    if (isAnimationEnabled_) {
        postEffectParameter.time += 1.0f / 60.0f;
    }
    if (postEffectParameter.time > 1000.0f) {
        postEffectParameter.time = 0.0f;
    }

    float boostKickStrength = SceneManager::GetInstance()->GetPostEffectKickStrength();
    postEffectParameter.boostKickStrength = boostKickStrength;

    if (isBoosting) {
        postEffectParameter.radialBlurSampleCount = 48;
        if (boostKickStrength > 0.0f) {
            postEffectParameter.radialBlurSampleCount = 64;
        }
        postEffectParameter.radialBlurWidth = 0.25f + 0.28f * boostKickStrength;
    } else {
        postEffectParameter.radialBlurSampleCount = 32;
        postEffectParameter.radialBlurWidth = 0.05f;
        postEffectParameter.boostKickStrength = 0.0f;
    }
}

void PostEffectManager::Apply(SceneManager* sceneManager, D3D12_GPU_DESCRIPTOR_HANDLE sceneColorHandle)
{
    if (sceneManager == nullptr) {
        CopyImageRenderer::PostEffectParameter& postEffectParameter = copyImageRenderer_->GetPostEffectParameter();
        postEffectParameter.radialBlurCenter = { 0.5f, 0.5f };

        SetBackBufferRenderTarget();
        ApplyPostEffectToCurrentTarget(PostEffectType::Copy, sceneColorHandle);
        return;
    }

    CopyImageRenderer::PostEffectParameter& postEffectParameter = copyImageRenderer_->GetPostEffectParameter();
    postEffectParameter.radialBlurCenter = sceneManager->GetPostEffectCenter();
    postEffectParameter.cameraShakeStrength = sceneManager->GetCameraShakeStrength();
    postEffectParameter.vignetteStrength = sceneManager->GetVignetteStrength();
    postEffectParameter.sonicBoomProgress = sceneManager->GetSonicBoomProgress();
    postEffectParameter.sonicBoomCenter = sceneManager->GetSonicBoomCenter();
    postEffectParameter.paintProgress = sceneManager->GetPaintProgress();
    postEffectParameter.paintIntensity = sceneManager->GetPaintIntensity();
    postEffectParameter.paintSeed = sceneManager->GetPaintSeed();
    postEffectParameter.paintPatternType = sceneManager->GetPaintPatternType();
    postEffectParameter.paintColor = sceneManager->GetPaintColor();

    const std::vector<PostEffectInfo>& postEffects = sceneManager->GetPostEffects();
    int enabledCount = 0;
    for (const PostEffectInfo& postEffect : postEffects) {
        if (postEffect.enabled) {
            enabledCount++;
        }
    }

    if (enabledCount == 0) {
        SetBackBufferRenderTarget();
        ApplyPostEffectToCurrentTarget(PostEffectType::Copy, sceneColorHandle);
        return;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE inputHandle = sceneColorHandle;
    uint32_t pingPongIndex = 0;
    int appliedCount = 0;

    for (const PostEffectInfo& postEffect : postEffects) {
        if (!postEffect.enabled) {
            continue;
        }

        appliedCount++;
        if (appliedCount == enabledCount) {
            if (postEffect.type == PostEffectType::Bloom && bloomRenderer_->IsEnabled()) {
                bloomRenderer_->Generate(inputHandle);
                SetBackBufferRenderTarget();
                bloomRenderer_->Composite(inputHandle);
                continue;
            }

            SetBackBufferRenderTarget();
            ApplyPostEffectToCurrentTarget(postEffect.type, inputHandle);
        } else {
            RenderTarget& renderTarget = pingPongRenderTargets_[pingPongIndex];
            if (postEffect.type == PostEffectType::Bloom && bloomRenderer_->IsEnabled()) {
                bloomRenderer_->Generate(inputHandle);
                renderTarget.BeginRender();
                bloomRenderer_->Composite(inputHandle);
                renderTarget.EndRender();

                inputHandle = renderTarget.GetSrvHandleGPU();
                pingPongIndex = GetNextPingPongIndex(pingPongIndex);
                continue;
            }

            renderTarget.BeginRender();
            ApplyPostEffectToCurrentTarget(postEffect.type, inputHandle);
            renderTarget.EndRender();

            inputHandle = renderTarget.GetSrvHandleGPU();
            pingPongIndex = GetNextPingPongIndex(pingPongIndex);
        }
    }
}

D3D12_CPU_DESCRIPTOR_HANDLE PostEffectManager::GetDepthDSVHandle() const
{
    return fogRenderer_->GetDepthDSVHandle();
}

D3D12_GPU_VIRTUAL_ADDRESS PostEffectManager::GetFogConstantBufferView() const
{
    return fogManager_->GetConstantBufferView();
}

void PostEffectManager::ApplyPostEffectToCurrentTarget(PostEffectType type, D3D12_GPU_DESCRIPTOR_HANDLE inputHandle)
{
    if (type == PostEffectType::Fog) {
        D3D12_GPU_VIRTUAL_ADDRESS fogConstantBufferView = fogManager_->GetConstantBufferView();
        if (fogConstantBufferView == 0) {
            copyImageRenderer_->SetPostEffectType(PostEffectType::Copy);
            copyImageRenderer_->Draw(inputHandle, fogRenderer_->GetDepthSRVHandle());
            return;
        }

        fogRenderer_->Apply(inputHandle, fogConstantBufferView);
        return;
    }

    if (type == PostEffectType::Bloom) {
        copyImageRenderer_->SetPostEffectType(PostEffectType::Copy);
        copyImageRenderer_->Draw(inputHandle, fogRenderer_->GetDepthSRVHandle());
        return;
    }

    copyImageRenderer_->SetPostEffectType(type);
    copyImageRenderer_->Draw(inputHandle, fogRenderer_->GetDepthSRVHandle());
}

void PostEffectManager::SetBackBufferRenderTarget()
{
    dxCommon_->SetBackBufferRenderTarget(dxCommon_->GetDSVHandle());
}

uint32_t PostEffectManager::GetNextPingPongIndex(uint32_t currentIndex) const
{
    uint32_t nextIndex = currentIndex + 1;
    if (nextIndex >= kPingPongRenderTargetCount) {
        nextIndex = 0;
    }

    return nextIndex;
}

void PostEffectManager::RenderTarget::Initialize(DirectXCommon* dxCommon, uint32_t rtvIndex)
{
    assert(dxCommon != nullptr);
    dxCommon_ = dxCommon;

    CreateResource();
    CreateViews(rtvIndex);

    viewport_.Width = static_cast<float>(WinApp::kClientWidth);
    viewport_.Height = static_cast<float>(WinApp::kClientHeight);
    viewport_.TopLeftX = 0.0f;
    viewport_.TopLeftY = 0.0f;
    viewport_.MinDepth = 0.0f;
    viewport_.MaxDepth = 1.0f;

    scissorRect_.left = 0;
    scissorRect_.top = 0;
    scissorRect_.right = WinApp::kClientWidth;
    scissorRect_.bottom = WinApp::kClientHeight;
}

void PostEffectManager::RenderTarget::BeginRender()
{
    Transition(D3D12_RESOURCE_STATE_RENDER_TARGET);

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
    commandList->RSSetViewports(1, &viewport_);
    commandList->RSSetScissorRects(1, &scissorRect_);
    commandList->OMSetRenderTargets(1, &rtvHandle_, false, nullptr);
    commandList->ClearRenderTargetView(rtvHandle_, clearColor_, 0, nullptr);
}

void PostEffectManager::RenderTarget::EndRender()
{
    Transition(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

D3D12_GPU_DESCRIPTOR_HANDLE PostEffectManager::RenderTarget::GetSrvHandleGPU() const
{
    return srvHandleGPU_;
}

void PostEffectManager::RenderTarget::CreateResource()
{
    ID3D12Device* device = dxCommon_->GetDevice();

    D3D12_RESOURCE_DESC resourceDesc {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDesc.Width = WinApp::kClientWidth;
    resourceDesc.Height = WinApp::kClientHeight;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = format_;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    D3D12_HEAP_PROPERTIES heapProperties {};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_CLEAR_VALUE clearValue {};
    clearValue.Format = format_;
    clearValue.Color[0] = clearColor_[0];
    clearValue.Color[1] = clearColor_[1];
    clearValue.Color[2] = clearColor_[2];
    clearValue.Color[3] = clearColor_[3];

    HRESULT result = device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        &clearValue,
        IID_PPV_ARGS(&resource_));
    assert(SUCCEEDED(result));

    currentState_ = D3D12_RESOURCE_STATE_RENDER_TARGET;
}

void PostEffectManager::RenderTarget::CreateViews(uint32_t rtvIndex)
{
    ID3D12Device* device = dxCommon_->GetDevice();

    rtvHandle_ = dxCommon_->GetRTVHandle(rtvIndex);

    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc {};
    rtvDesc.Format = format_;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    device->CreateRenderTargetView(resource_.Get(), &rtvDesc, rtvHandle_);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc {};
    srvDesc.Format = format_;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    srvIndex_ = SrvManager::GetInstance()->Allocate();
    srvHandleCPU_ = SrvManager::GetInstance()->GetCPUDescriptorHandle(srvIndex_);
    srvHandleGPU_ = SrvManager::GetInstance()->GetGPUDescriptorHandle(srvIndex_);
    device->CreateShaderResourceView(resource_.Get(), &srvDesc, srvHandleCPU_);
}

void PostEffectManager::RenderTarget::Transition(D3D12_RESOURCE_STATES nextState)
{
    if (currentState_ == nextState) {
        return;
    }

    D3D12_RESOURCE_BARRIER barrier {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = resource_.Get();
    barrier.Transition.StateBefore = currentState_;
    barrier.Transition.StateAfter = nextState;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    dxCommon_->GetCommandList()->ResourceBarrier(1, &barrier);
    currentState_ = nextState;
}

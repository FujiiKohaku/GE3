#include "Engine/Renderer/Renderer.h"

#include "App/Scene/SceneManager.h"
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
#include "Engine/input/Input.h"
#include "Engine/Shadow/ShadowMapRenderer.h"
#include "Engine/Light/LightManager.h"
#include "Engine/PostEffect/Volumetric/VolumetricLightRenderer.h"

Renderer::Renderer() = default;

Renderer::~Renderer()
{
    Object3dManager::GetInstance()->SetShadowRenderer(nullptr);
}

void Renderer::Initialize()
{
    // Offscreen renderer setup
    offscreenRenderer_ = std::make_unique<OffscreenRenderer>();
    offscreenRenderer_->Initialize();

    // Post effect chain setup
    postEffectManager_ = std::make_unique<PostEffectManager>();
    postEffectManager_->Initialize(DirectXCommon::GetInstance());
    postEffectManager_->SetNormalTextureHandle(
        offscreenRenderer_->GetNormalSrvHandleGPU());
}

void Renderer::Update()
{
    Camera* defaultCamera = Object3dManager::GetInstance()->GetDefaultCamera();
    postEffectManager_->Update(defaultCamera);
}

void Renderer::DrawImGui()
{
    postEffectManager_->DrawImGui();
}

void Renderer::Draw(SceneManager* sceneManager)
{
    FontManager::GetInstance()->FlushAtlasUpdates();
    // シーンやモデルが予約したテクスチャ転送を、描画前に一度だけまとめて実行する。
    TextureManager::GetInstance()->FlushUploads();

    // SRV heap setup
    SrvManager::GetInstance()->PreDraw();

    Camera* defaultCamera = Object3dManager::GetInstance()->GetDefaultCamera();
    EffectManager* effectManager = EffectManager::GetInstance();
    if (effectManager->IsInitialized()) {
        if (defaultCamera != nullptr) {
            effectManager->SetCamera(defaultCamera);
            effectManager->UpdatePerView();
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

    // Always clear/supply frame inputs, including frames with shadows disabled.
    const ShadowMapRenderer* volumetricShadows = nullptr;
    if (shadows.enabled) { volumetricShadows = shadowRenderer_.get(); }
    postEffectManager_->GetVolumetricLightRenderer()->SetFrameInputs(defaultCamera, volumetricShadows);

    // Offscreen draw start
    postEffectManager_->PreDrawDepth();
    offscreenRenderer_->SetClearColor(sceneManager->GetSceneClearColor());
    offscreenRenderer_->PreDraw(postEffectManager_->GetDepthDSVHandle());
    sceneManager->Draw3D();
    DebugRenderer::GetInstance()->Draw();
    postEffectManager_->PostDrawDepth();
    offscreenRenderer_->PostDraw();

    // BackBuffer draw start
    DirectXCommon::GetInstance()->PreDraw();
    
    // Post effect apply
    const bool isBoosting = Input::GetInstance()->IsKeyPressed(DIK_LSHIFT);
    postEffectManager_->SetBoostRadialBlurParameters(isBoosting);
    postEffectManager_->PrepareSceneForParticleDraw(
        sceneManager,
        offscreenRenderer_->GetSrvHandleGPU());

    postEffectManager_->PrepareDepthForParticleDraw();
    postEffectManager_->BeginParticleDraw();
    sceneManager->DrawParticle();
    postEffectManager_->EndParticleDraw();
    postEffectManager_->PostDrawDepth();

    postEffectManager_->ApplyAfterParticleDraw(sceneManager);

    // 2D draw
    sceneManager->Draw2D();

    // ImGui is not initialized in the Release configuration.
#ifdef USE_IMGUI
    ImGuiManager::GetInstance()->Draw();
#endif

    ScreenshotManager::GetInstance()->DrawNotification();
    ScreenshotManager::GetInstance()->PrepareCapture();

    // Present
    DirectXCommon::GetInstance()->PostDraw();
    ScreenshotManager::GetInstance()->CompleteCapture();
}

#include "Engine/Renderer/Renderer.h"

#include "App/Scene/SceneManager.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/Camera/Camera.h"
#include "Engine/Debug/DebugRenderer.h"
#include "Engine/DirectXCommon/DirectXCommon.h"
#include "Engine/Effect/EffectManager.h"
#include "Engine/Fog/FogManager.h"
#include "Engine/Fog/FogRenderer.h"
#include "Engine/ImGuiManager/ImGuiManager.h"
#include "Engine/Particle/ParticleManager.h"
#include "Engine/PostEffect/CopyImageRenderer.h"
#include "Engine/PostEffect/OffscreenRenderer.h"
#include "Engine/SrvManager/SrvManager.h"
#include "Engine/input/Input.h"

Renderer::Renderer() = default;

Renderer::~Renderer() = default;

void Renderer::Initialize()
{
    // Offscreen renderer setup
    offscreenRenderer_ = std::make_unique<OffscreenRenderer>();
    offscreenRenderer_->Initialize();

    // CopyImage renderer setup
    copyImageRenderer_ = std::make_unique<CopyImageRenderer>();
    copyImageRenderer_->Initialize(DirectXCommon::GetInstance());

    // Fog data setup
    fogManager_ = std::make_unique<FogManager>();
    fogManager_->Initialize(DirectXCommon::GetInstance());

    // Fog post effect setup
    fogRenderer_ = std::make_unique<FogRenderer>();
    fogRenderer_->Initialize(DirectXCommon::GetInstance());
}

void Renderer::Update()
{
    // Fog camera info update
    Camera* defaultCamera = Object3dManager::GetInstance()->GetDefaultCamera();
    if (defaultCamera != nullptr) {
        fogManager_->SetCameraInfo(
            defaultCamera->GetNearClip(),
            defaultCamera->GetFarClip(),
            defaultCamera->GetFovY(),
            defaultCamera->GetAspectRatio());
    }

    fogManager_->Update();
}

void Renderer::DrawImGui()
{
    fogManager_->DrawImGui();
}

void Renderer::Draw(SceneManager* sceneManager)
{
    // SRV heap setup
    SrvManager::GetInstance()->PreDraw();

    Camera* defaultCamera = Object3dManager::GetInstance()->GetDefaultCamera();
    if (defaultCamera != nullptr) {
        EffectManager::GetInstance()->SetCamera(defaultCamera);
        ParticleManager::GetInstance()->SetCamera(defaultCamera);
        EffectManager::GetInstance()->UpdatePerView();
        ParticleManager::GetInstance()->UpdatePerView();
    }

    D3D12_GPU_VIRTUAL_ADDRESS fogConstantBufferView = fogManager_->GetConstantBufferView();
    EffectManager::GetInstance()->SetFogConstantBufferView(fogConstantBufferView);
    ParticleManager::GetInstance()->SetFogConstantBufferView(fogConstantBufferView);

    // Offscreen draw start
    fogRenderer_->PreDrawDepth();
    offscreenRenderer_->PreDraw(fogRenderer_->GetDepthDSVHandle());
    sceneManager->Draw3D();
    DebugRenderer::GetInstance()->Draw();
    fogRenderer_->PostDrawDepth();
    offscreenRenderer_->PostDraw();

    // BackBuffer draw start
    DirectXCommon::GetInstance()->PreDraw();
    
    // Post effect apply
    copyImageRenderer_->SetPostEffectType(sceneManager->GetPostEffectType());
    CopyImageRenderer::PostEffectParameter& postEffectParameter = copyImageRenderer_->GetPostEffectParameter();
    const bool isBoosting = Input::GetInstance()->IsKeyPressed(DIK_LSHIFT);
    if (isBoosting) {
        postEffectParameter.radialBlurSampleCount = 48;
        postEffectParameter.radialBlurWidth = 0.25f;
    } else {
        postEffectParameter.radialBlurSampleCount = 32;
        postEffectParameter.radialBlurWidth = 0.05f;
    }

    copyImageRenderer_->Draw(offscreenRenderer_->GetSrvHandleGPU(), fogRenderer_->GetDepthSRVHandle());
    
    // ポストエフェクトが無効（Copy）かつフォグが有効な場合のみ、フォグ描画を適用する
    if (sceneManager->GetPostEffectType() == PostEffectType::Copy && fogManager_->GetFogData().isEnabled != 0) {
        fogRenderer_->Apply(offscreenRenderer_->GetSrvHandleGPU(), fogConstantBufferView);
    }

    // Particle draw
    fogRenderer_->PrepareDepthForParticleDraw();
    DirectXCommon::GetInstance()->SetBackBufferRenderTarget(fogRenderer_->GetDepthDSVHandle());
    sceneManager->DrawParticle();

    // 2D draw
    sceneManager->Draw2D();

    // ImGui draw
    ImGuiManager::GetInstance()->Draw();

    // Present
    DirectXCommon::GetInstance()->PostDraw();
}

#include "Engine/Renderer/Renderer.h"

#include "App/Scene/SceneManager.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/Camera/Camera.h"
#include "Engine/Debug/DebugRenderer.h"
#include "Engine/DirectXCommon/DirectXCommon.h"
#include "Engine/Effect/EffectManager.h"
#include "Engine/ImGuiManager/ImGuiManager.h"
#include "Engine/Particle/ParticleManager.h"
#include "Engine/PostEffect/OffscreenRenderer.h"
#include "Engine/PostEffect/PostEffectManager.h"
#include "Engine/SrvManager/SrvManager.h"
#include "Engine/input/Input.h"

Renderer::Renderer() = default;

Renderer::~Renderer() = default;

void Renderer::Initialize()
{
    // Offscreen renderer setup
    offscreenRenderer_ = std::make_unique<OffscreenRenderer>();
    offscreenRenderer_->Initialize();

    // Post effect chain setup
    postEffectManager_ = std::make_unique<PostEffectManager>();
    postEffectManager_->Initialize(DirectXCommon::GetInstance());
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
    // SRV heap setup
    SrvManager::GetInstance()->PreDraw();

    Camera* defaultCamera = Object3dManager::GetInstance()->GetDefaultCamera();
    if (defaultCamera != nullptr) {
        EffectManager::GetInstance()->SetCamera(defaultCamera);
        ParticleManager::GetInstance()->SetCamera(defaultCamera);
        EffectManager::GetInstance()->UpdatePerView();
        ParticleManager::GetInstance()->UpdatePerView();
    }

    D3D12_GPU_VIRTUAL_ADDRESS fogConstantBufferView = postEffectManager_->GetFogConstantBufferView();
    EffectManager::GetInstance()->SetFogConstantBufferView(fogConstantBufferView);
    ParticleManager::GetInstance()->SetFogConstantBufferView(fogConstantBufferView);

    // Offscreen draw start
    postEffectManager_->PreDrawDepth();
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
    postEffectManager_->Apply(sceneManager, offscreenRenderer_->GetSrvHandleGPU());

    // Particle draw
    postEffectManager_->PrepareDepthForParticleDraw();
    DirectXCommon::GetInstance()->SetBackBufferRenderTarget(postEffectManager_->GetDepthDSVHandle());
    sceneManager->DrawParticle();

    // 2D draw
    sceneManager->Draw2D();

    // ImGui draw
    ImGuiManager::GetInstance()->Draw();

    // Present
    DirectXCommon::GetInstance()->PostDraw();
}

#include "Engine/Renderer/Renderer.h"

#include "App/Scene/SceneManager.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/Camera/Camera.h"
#include "Engine/Debug/DebugRenderer.h"
#include "Engine/DirectXCommon/DirectXCommon.h"
#include "Engine/Effect/EffectManager.h"
#include "Engine/ImGuiManager/ImGuiManager.h"
#include "Engine/PostEffect/OffscreenRenderer.h"
#include "Engine/PostEffect/PostEffectManager.h"
#include "Engine/SrvManager/SrvManager.h"
#include "Engine/TextureManager/TextureManager.h"
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
    // シーンやモデルが予約したテクスチャ転送を、描画前に一度だけまとめて実行する。
    TextureManager::GetInstance()->FlushUploads();

    // SRV heap setup
    SrvManager::GetInstance()->PreDraw();

    Camera* defaultCamera = Object3dManager::GetInstance()->GetDefaultCamera();
    if (defaultCamera != nullptr) {
        EffectManager::GetInstance()->SetCamera(defaultCamera);
        EffectManager::GetInstance()->UpdatePerView();
    }

    D3D12_GPU_VIRTUAL_ADDRESS fogConstantBufferView = postEffectManager_->GetFogConstantBufferView();
    EffectManager::GetInstance()->SetFogConstantBufferView(fogConstantBufferView);

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

    // ImGui is not initialized in the Release configuration.
#ifdef USE_IMGUI
    ImGuiManager::GetInstance()->Draw();
#endif

    // Present
    DirectXCommon::GetInstance()->PostDraw();
}

#include "LoadingScene.h"

#include "Engine/2D/SpriteManager.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/3D/SkinningObject3dManager.h"
#include "Engine/3D/SkyBox/SkyBoxManager.h"
#include "Engine/Debug/DebugRenderer.h"
#include "Engine/DirectXCommon/DirectXCommon.h"
#include "Engine/Effect/EffectManager.h"
#include "Engine/Light/LightManager.h"
#include "Engine/SrvManager/SrvManager.h"
#include "Engine/TextureManager/TextureManager.h"
#include "GamePlayScene.h"
#include "SceneManager.h"

namespace {
constexpr float kProgressBarWidth = 720.0f;
constexpr float kProgressBarHeight = 28.0f;
constexpr float kProgressBarLeft = 280.0f;
constexpr float kProgressBarTop = 540.0f;
constexpr const char* kWhiteTexturePath = "resources/Textures/white.png";
}

void LoadingScene::Initialize()
{
    // 起動時から遅延ロードされたデフォルトテクスチャ
    TextureManager::GetInstance()->LoadTexture("resources/Textures/uvChecker.png");
    TextureManager::GetInstance()->LoadTexture("resources/Textures/fence.png");
    TextureManager::GetInstance()->LoadTexture("resources/Textures/BaseColor_Cube.png");
    TextureManager::GetInstance()->LoadTexture("resources/Textures/noise0.png");

    SceneManager::GetInstance()->SetPostEffectType(PostEffectType::Copy);

    backgroundSprite_ = std::make_unique<Sprite>();
    backgroundSprite_->Initialize(SpriteManager::GetInstance(), kWhiteTexturePath);
    backgroundSprite_->SetSize({ 1280.0f, 720.0f });
    backgroundSprite_->SetColor({ 0.015f, 0.02f, 0.04f, 1.0f });

    progressBackSprite_ = std::make_unique<Sprite>();
    progressBackSprite_->Initialize(SpriteManager::GetInstance(), kWhiteTexturePath);
    progressBackSprite_->SetPosition({ kProgressBarLeft, kProgressBarTop });
    progressBackSprite_->SetSize({ kProgressBarWidth, kProgressBarHeight });
    progressBackSprite_->SetColor({ 0.12f, 0.15f, 0.22f, 1.0f });

    progressSprite_ = std::make_unique<Sprite>();
    progressSprite_->Initialize(SpriteManager::GetInstance(), kWhiteTexturePath);
    progressSprite_->SetPosition({ kProgressBarLeft, kProgressBarTop });
    progressSprite_->SetSize({ 0.0f, kProgressBarHeight });
    progressSprite_->SetColor({ 0.25f, 0.75f, 1.0f, 1.0f });

}

void LoadingScene::Update()
{
    if (!hasDrawnFirstFrame_) {
        hasDrawnFirstFrame_ = true;
    } else if (!gameResourcesInitialized_) {
        EffectManager* effectManager = EffectManager::GetInstance();
        if (!effectManager->IsInitialized()) {
            DirectXCommon* dxCommon = DirectXCommon::GetInstance();
            Object3dManager::GetInstance()->Initialize(dxCommon);
            SkinningObject3dManager::GetInstance()->Initialize(dxCommon);
            SkyBoxManager::GetInstance()->Initialize(dxCommon);
            LightManager::GetInstance()->Initialize(dxCommon);
            DebugRenderer::GetInstance()->Initialize();
            effectManager->Initialize(
                dxCommon,
                SrvManager::GetInstance(),
                nullptr);
        }
        effectManager->BeginWarmUp();
        gameResourcesInitialized_ = true;
    } else {
        EffectManager::GetInstance()->UpdateWarmUp();
    }

    float progress = 0.0f;
    if (gameResourcesInitialized_) {
        progress = EffectManager::GetInstance()->GetWarmUpProgress();
    }
    progressSprite_->SetSize({ kProgressBarWidth * progress, kProgressBarHeight });

    backgroundSprite_->Update();
    progressBackSprite_->Update();
    progressSprite_->Update();

    if (gameResourcesInitialized_ &&
        EffectManager::GetInstance()->IsWarmUpComplete()) {
        SceneManager::GetInstance()->SetNextScene(std::move(nextScene_));
    }
}

void LoadingScene::Draw2D()
{
    SpriteManager::GetInstance()->PreDraw();
    backgroundSprite_->Draw();
    progressBackSprite_->Draw();
    progressSprite_->Draw();
}

void LoadingScene::Draw3D() {}
void LoadingScene::DrawParticle() {}
void LoadingScene::DrawImGui() {}
void LoadingScene::Finalize() {}

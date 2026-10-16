// 次のステージのリソースや文字を準備し、読み込みが完了したらプレイ画面へ切り替える。
#include "App/Scene/Loading/LoadingScene.h"

#include "Engine/2D/SpriteManager.h"
#include "Engine/2D/Text/TextRenderer.h"
#include "App/Game/Tutorial/TutorialController.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/3D/SkinningObject3dManager.h"
#include "Engine/3D/SkyBox/SkyBoxManager.h"
#include "Engine/Debug/DebugRenderer.h"
#include "Engine/DirectXCommon/DirectXCommon.h"
#include "Engine/Effect/EffectManager.h"
#include "Engine/Light/LightManager.h"
#include "Engine/SrvManager/SrvManager.h"
#include "Engine/TextureManager/TextureManager.h"
#include "App/Scene/Gameplay/GamePlayScene.h"
#include "App/Scene/Common/SceneManager.h"

namespace {
constexpr float kProgressBarWidth = 720.0f;
constexpr float kProgressBarHeight = 28.0f;
constexpr float kProgressBarLeft = 280.0f;
constexpr float kProgressBarTop = 540.0f;
constexpr const char* kWhiteTexturePath = "resources/Textures/white.png";
}

// 読み込み画面を準備し、遷移先に必要なリソースの読み込みを開始する。
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
    if (simulationBoot_) {
        simulationText_ = std::make_unique<Text>();
        simulationText_->Initialize("resources/Fonts/NotoSansJP/NotoSansJP-Variable.ttf");
        simulationText_->SetText("SIMULATION INITIALIZING");
        simulationText_->SetAnchorPoint({ 0.5f, 0.5f });
        simulationText_->SetPosition({ 640.0f, 560.0f });
        simulationText_->SetFontSize(26.0f);
        simulationText_->SetColor({ 0.35f, 0.90f, 1.0f, 1.0f });
        simulationText_->Update();
        progressBackSprite_->SetPosition({ 430.0f, 602.0f });
        progressBackSprite_->SetSize({ 420.0f, 3.0f });
        progressSprite_->SetPosition({ 430.0f, 602.0f });
    }

}

// 読み込みの完了を確認し、必要な文字も準備してから遷移先へ進む。
void LoadingScene::Update()
{
    if (!hasDrawnFirstFrame_) {
        hasDrawnFirstFrame_ = true;
    } else if (simulationBoot_ && !hasPreparedTutorialFont_) {
        FontManager* fontManager = FontManager::GetInstance();
        const FontHandle fontHandle = fontManager->LoadFont(
            "resources/Fonts/NotoSansJP/NotoSansJP-Variable.ttf");
        fontManager->EnsureGlyphs(fontHandle, TutorialController::GetGlyphText());
        // このフレームの描画前にアトラス更新・転送を終えてから、次の準備へ進む。
        hasPreparedTutorialFont_ = true;
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
    float progressWidth = kProgressBarWidth;
    float progressHeight = kProgressBarHeight;
    if (simulationBoot_) {
        progressWidth = 420.0f;
        progressHeight = 3.0f;
    }
    progressSprite_->SetSize({ progressWidth * progress, progressHeight });

    backgroundSprite_->Update();
    progressBackSprite_->Update();
    progressSprite_->Update();

    if (gameResourcesInitialized_ &&
        EffectManager::GetInstance()->IsWarmUpComplete()) {
        SceneManager::GetInstance()->SetNextScene(std::move(nextScene_));
    }
}

// 読み込み中の表示を画面へ描画する。
void LoadingScene::Draw2D()
{
    SpriteManager::GetInstance()->PreDraw();
    backgroundSprite_->Draw();
    progressBackSprite_->Draw();
    progressSprite_->Draw();
    if (simulationText_) {
        TextRenderer::GetInstance()->PreDraw();
        simulationText_->Draw();
    }
}

// このクラスではこの描画経路を使用しないため、処理を行わない。
void LoadingScene::Draw3D() {}
// このクラスではこの描画経路を使用しないため、処理を行わない。
void LoadingScene::DrawParticle() {}
// このクラスではこの描画経路を使用しないため、処理を行わない。
void LoadingScene::DrawImGui() {}
void LoadingScene::Finalize() {}

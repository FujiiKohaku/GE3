#include "GamePlayScene.h"
#include "App/Game/Enemy/MoveEnemy/MoveEnemy.h"
#include "App/Game/Enemy/PaintEnemy/PaintShooterEnemy.h"
#include "App/Game/Enemy/Bullet/PaintBullet.h"
#include "App/Game/Enemy/SwarmEnemy/SwarmEnemy.h"
#include "Engine/Animation/AnimationLoder.h"
#include "Engine/CollisionManager/CollisionManager.h"
#include "Engine/Effect/EffectManager.h"
#include "Engine/Light/LightManager.h"
#include "Engine/math/MathStruct.h"
#include "Engine/audio/SoundManager.h"
#include <cstdlib>
#include <numbers>

#include "SceneManager.h"

#include "../externals/json.hpp"
#include "Engine/PostEffect/PostEffectType.h"
#include <fstream>
#include <string_view>

#include "../../Engine/LevelEditor/LevelDataLoader.h"
#include "../../Engine/CollisionManager/BoxCollider.h"

#include "ClearScene.h"
#include "GameOverScene.h"
#include "TitleScene.h"
#include "Engine/Debug/DebugRenderer.h"
#include "Engine/Logger/Logger.h"
#include "Engine/Input/Input.h"
#include "Engine/Time/TimeManager.h"
#include "DevelopmentWebPanel.h"
#include <algorithm>
#include <cmath>

namespace {
Player::ControlMode gControlMode = Player::ControlMode::KeyboardAndMouse;
float gMouseSensitivity = 1.0f;

// 機能色。ゲーム内エフェクトとHUDで同じ意味に同じ色を使う。
constexpr Vector4 kPlayerActionCyan { 0.325f, 0.847f, 0.910f, 1.0f };
constexpr Vector4 kDangerVermilion { 1.000f, 0.353f, 0.239f, 1.0f };
constexpr Vector4 kRecoveryMint { 0.475f, 0.902f, 0.702f, 1.0f };
constexpr Vector4 kHudIvory { 0.910f, 0.870f, 0.750f, 1.0f };
constexpr Vector4 kHudBrass { 0.714f, 0.541f, 0.290f, 1.0f };
constexpr Vector4 kHudPanel { 0.025f, 0.055f, 0.100f, 0.84f };

void AddHudFramePart(
    std::vector<std::unique_ptr<Sprite>>& sprites,
    const Vector2& position,
    const Vector2& size,
    const Vector4& color)
{
    auto sprite = std::make_unique<Sprite>();
    sprite->Initialize(SpriteManager::GetInstance(), "resources/Textures/white.png");
    sprite->SetAnchorPoint({ 0.0f, 0.0f });
    sprite->SetPosition(position);
    sprite->SetSize(size);
    sprite->SetColor(color);
    sprite->Update();
    sprites.push_back(std::move(sprite));
}

void InitializeHudFrame(
    std::vector<std::unique_ptr<Sprite>>& sprites,
    const Vector2& position,
    const Vector2& size,
    const Vector2& anchor,
    const Vector4& accentColor)
{
    sprites.clear();
    const Vector2 topLeft {
        position.x - size.x * anchor.x,
        position.y - size.y * anchor.y
    };
    constexpr float kBorder = 3.0f;
    constexpr float kCornerLength = 28.0f;

    AddHudFramePart(sprites, topLeft, size, kHudPanel);
    AddHudFramePart(sprites, topLeft, { size.x, kBorder }, kHudIvory);
    AddHudFramePart(sprites, { topLeft.x, topLeft.y + size.y - kBorder }, { size.x, kBorder }, kHudIvory);
    AddHudFramePart(sprites, topLeft, { kBorder, size.y }, kHudIvory);
    AddHudFramePart(sprites, { topLeft.x + size.x - kBorder, topLeft.y }, { kBorder, size.y }, kHudIvory);
    AddHudFramePart(sprites, topLeft, { kCornerLength, 6.0f }, accentColor);
    AddHudFramePart(
        sprites,
        { topLeft.x + size.x - kCornerLength, topLeft.y + size.y - 6.0f },
        { kCornerLength, 6.0f },
        accentColor);
}

void DrawHudFrame(const std::vector<std::unique_ptr<Sprite>>& sprites)
{
    for (const std::unique_ptr<Sprite>& sprite : sprites) {
        sprite->Draw();
    }
}

Vector2 ScreenPositionToPostEffectCenter(const Vector2& screenPosition, float clientWidth, float clientHeight)
{
    Vector2 center {};
    center.x = 0.5f;
    center.y = 0.5f;

    if (clientWidth <= 0.0f) {
        return center;
    }

    if (clientHeight <= 0.0f) {
        return center;
    }

    center.x = screenPosition.x / clientWidth;
    center.y = screenPosition.y / clientHeight;
    return center;
}

}

void GamePlayScene::Initialize()
{
    EnemyBullet::SetTimeScale(1.0f);
    BaseEnemy::SetBulletManager(&enemyBulletManager_);
    enemyBulletManager_.Clear();
    StageCatalog* stageCatalog = StageCatalog::GetInstance();
    if (!stageCatalog->Load()) {
        Logger::Log(stageCatalog->GetLastError());
    }
    const StageSettings* selectedStage = stageCatalog->Find(stageId_);
    if (selectedStage == nullptr) {
        selectedStage = stageCatalog->Find("stage01");
    }
    if (selectedStage != nullptr) {
        stageSettings_ = *selectedStage;
        stageId_ = stageSettings_.id;
    } else {
        stageSettings_.id = "stage01";
        stageSettings_.layoutFile = "resources/Scenes/stage01.json";
        stageSettings_.bossRailAutoExtension = true;
        stageSettings_.swarmWaveDistances = {
            260.0f, 620.0f, 980.0f, 1340.0f, 1560.0f, 1740.0f };
        stageSettings_.recoveryItemPositions = {
            { -5.0f, 1.5f, 410.0f },
            { 5.0f, 1.5f, 1040.0f },
            { 0.0f, 5.0f, 1700.0f } };
    }
    railSpeed_ = stageSettings_.railSpeed;

    editorManager_ = std::make_unique<EditorManager>();
    editorManager_->Initialize();
    sceneObjectManager_ = std::make_unique<SceneObjectManager>();
    gameplayCollisionSystem_ = std::make_unique<GameplayCollisionSystem>();
    rail_ = std::make_unique<Rail>();
    rail_->Initialize();
    if (!stageSettings_.railControlPoints.empty()) {
        for (const Vector3& point : stageSettings_.railControlPoints) {
            rail_->AddPoint(point);
        }
    } else {
        for (float z = 0.0f;
             z < stageSettings_.railLength;
             z += stageSettings_.railPointInterval) {
            rail_->AddPoint({ 0.0f, 0.0f, z });
        }
        if (stageSettings_.railLength > 0.0f) {
            rail_->AddPoint({ 0.0f, 0.0f, stageSettings_.railLength });
        }
    }
    // 通常画面は輪郭・霧・ブルームだけに固定する。
    ConfigureGameplayPostEffects(false);
    // =================================================
    // Camera
    // =================================================
    Logger::Log("GamePlayScene::Initialize: Starting camera initialization");
    camera_ = std::make_unique<Camera>();
    camera_->Initialize();
    camera_->SetTranslate({ 0.0f, 3.0f, -30.0f });
    camera_->SetRotate({ 0.0f, 0.0f, 0.0f });
    normalFovY_ = camera_->GetFovY();
    currentFovY_ = normalFovY_;

    Logger::Log("GamePlayScene::Initialize: Starting aimCamera initialization");
    aimCamera_ = std::make_unique<Camera>();
    aimCamera_->Initialize();
    aimCamera_->SetTranslate({ 0.0f, 3.0f, -30.0f });
    aimCamera_->SetRotate({ 0.0f, 0.0f, 0.0f });
    Logger::Log("GamePlayScene::Initialize: aimCamera initialized successfully");

    POINT centerMousePosition;
    centerMousePosition.x = WinApp::GetInstance()->kClientWidth / 2;
    centerMousePosition.y = WinApp::GetInstance()->kClientHeight / 2;

    ClientToScreen(WinApp::GetInstance()->GetHwnd(), &centerMousePosition);
    SetCursorPos(centerMousePosition.x, centerMousePosition.y);
    debugCameraController_ = std::make_unique<DebugCameraController>();
    debugCameraController_->SetTargetCamera(camera_.get());

    SkinningObject3dManager::GetInstance()->SetDefaultCamera(camera_.get());
    Object3dManager::GetInstance()->SetDefaultCamera(camera_.get());
    // =================================================
    // Managers
    // =================================================
    // EffectManager本体はゲーム起動時に初期化済みなので、
    // このシーンで使用するカメラだけを設定する。
    EffectManager::GetInstance()->SetCamera(camera_.get());
    // =================================================
    // SkinningObject3d
    // =================================================

    TextureManager::GetInstance()->LoadTexture("resources/Textures/BaseColor_Cube.png");
    TextureManager::GetInstance()->LoadTexture("resources/Textures/uvChecker.png");
    TextureManager::GetInstance()->LoadTexture(stageSettings_.skybox);
    TextureManager::GetInstance()->LoadTexture("resources/Textures/aim.png");

    // nodeLoad
    ModelManager::GetInstance()->Load("Characters/Enemy/Drone/dolone.obj");
    ModelManager::GetInstance()->Load("Characters/Animation/SneakWalk/sneakWalk.gltf");
    Model* recoveryItemModel = ModelManager::GetInstance()->Load("Debug/Samples/AnimatedCube/AnimatedCube.gltf");
    Model* playerModel = ModelManager::GetInstance()->Load("fish/fish.obj");

    // エネミー・弾モデル
    enemyModel_ = ModelManager::GetInstance()->Load("Debug/baikinMusi/baikinMusi.obj");
    enemyBulletModel_ = ModelManager::GetInstance()->Load("Debug/block/block.obj");
    fearWormEnemyModel_ = ModelManager::GetInstance()->Load("Debug/Sphere/sphere.obj");
    angerBlockModel_ = ModelManager::GetInstance()->Load("Environment/Block/block.obj");
    // animationskinLoad
    // skinningWalk
    ModelManager::GetInstance()->Load("Characters/Animation/Walk/walk.gltf");
    //==============
    //  OBJ
    //==============
    Object3d* terrain_ = sceneObjectManager_->CreateObject("terrain", "Environment/Terrain/terrain.obj");

    Object3d* star = sceneObjectManager_->CreateObject("star", "Weapons/Star/star.obj");

    animationActor_ = std::make_unique<AnimationActor>();
    OutputDebugStringA("A\n");
    animationActor_->Initialize("Characters/Animation/SneakWalk/sneakWalk.gltf");
    OutputDebugStringA("B\n");
    animationActor_->SetRotate({ 0.0f, std::numbers::pi_v<float>, 0.0f });
    animationActor_->SetTranslate({ 5.0f, -2.0f, 0.0f });
    animationActor_->SetScale({ 1.0f, 1.0f, 1.0f });

    // =================================================
    // Particle
    // =================================================

    EulerTransform t { };
    t.translate = { 0.0f, 0.0f, 0.0f };
    t.scale = { 100.0f, 100.0f, 100.0f };
    Vector3 position { 0.0f, 1.0f, 0.0f };

    // =================================================
    // Light
    // =================================================

    ApplyStageVisualPreset();

    // =================================================
    // Sound
    // =================================================
    // bgm = SoundManager::GetInstance()->SoundLoadFile("resources/Sounds/BGM.wav");
    // SoundManager::GetInstance()->SoundPlayWave(bgm);

    /*testSprite_ = std::make_unique<Sprite>();
    testSprite_->Initialize(SpriteManager::GetInstance(), "resources/Textures/uvChecker.png");*/

    Logger::Log("GamePlayScene::Initialize: Allocating aimSprite");
    aimSprite_ = std::make_unique<Sprite>();
    Logger::Log("GamePlayScene::Initialize: Initializing aimSprite");
    aimSprite_->Initialize(SpriteManager::GetInstance(), "resources/Textures/aim.png");
    Logger::Log("GamePlayScene::Initialize: Setting aimSprite size");
    aimSprite_->SetSize({ 128.0f, 128.0f });
    Logger::Log("GamePlayScene::Initialize: Setting aimSprite anchor");
    aimSprite_->SetAnchorPoint({ 0.5f, 0.5f });
    Logger::Log("GamePlayScene::Initialize: Setting aimSprite position");
    aimSprite_->SetPosition({ WinApp::GetInstance()->kClientWidth / 2.0f, WinApp::GetInstance()->kClientHeight / 2.0f });
    Logger::Log("GamePlayScene::Initialize: Updating aimSprite");
    aimSprite_->Update();

    constexpr size_t kHomingMarkerCount = 6;
    homingLockSprites_.reserve(kHomingMarkerCount);
    for (size_t index = 0; index < kHomingMarkerCount; ++index) {
        auto marker = std::make_unique<Sprite>();
        marker->Initialize(SpriteManager::GetInstance(), "resources/Textures/aim.png");
        marker->SetSize({ 84.0f, 84.0f });
        marker->SetAnchorPoint({ 0.5f, 0.5f });
        marker->SetColor({ 1.0f, 0.15f, 0.05f, 0.95f });
        marker->Update();
        homingLockSprites_.push_back(std::move(marker));
    }

    // ホワイトPNG (resources/Textures/white.png) を使用した縦長ポーズUIスプライトの初期化
    TextureManager::GetInstance()->LoadTexture("resources/Textures/white.png");

    // 1. 縦長背景パネル (280x380)
    pauseMenuPanelSprite_ = std::make_unique<Sprite>();
    pauseMenuPanelSprite_->Initialize(SpriteManager::GetInstance(), "resources/Textures/white.png");
    pauseMenuPanelSprite_->SetSize({ 400.0f, 520.0f });
    pauseMenuPanelSprite_->SetAnchorPoint({ 0.5f, 0.5f });
    pauseMenuPanelSprite_->SetPosition({ WinApp::GetInstance()->kClientWidth / 2.0f, WinApp::GetInstance()->kClientHeight / 2.0f });
    pauseMenuPanelSprite_->SetColor({ 0.06f, 0.06f, 0.09f, 0.92f });
    pauseMenuPanelSprite_->Update();

    // 2. 「再開」ボタン用枠
    pauseResumeBtnSprite_ = std::make_unique<Sprite>();
    pauseResumeBtnSprite_->Initialize(SpriteManager::GetInstance(), "resources/Textures/white.png");
    pauseResumeBtnSprite_->SetSize({ 220.0f, 44.0f });
    pauseResumeBtnSprite_->SetAnchorPoint({ 0.5f, 0.5f });
    pauseResumeBtnSprite_->SetPosition({ WinApp::GetInstance()->kClientWidth / 2.0f, WinApp::GetInstance()->kClientHeight / 2.0f - 70.0f });
    pauseResumeBtnSprite_->SetColor({ 0.18f, 0.45f, 0.75f, 0.90f });
    pauseResumeBtnSprite_->Update();

    // 3. 「リトライ」ボタン用枠
    pauseRetryBtnSprite_ = std::make_unique<Sprite>();
    pauseRetryBtnSprite_->Initialize(SpriteManager::GetInstance(), "resources/Textures/white.png");
    pauseRetryBtnSprite_->SetSize({ 220.0f, 44.0f });
    pauseRetryBtnSprite_->SetAnchorPoint({ 0.5f, 0.5f });
    pauseRetryBtnSprite_->SetPosition({ WinApp::GetInstance()->kClientWidth / 2.0f, WinApp::GetInstance()->kClientHeight / 2.0f });
    pauseRetryBtnSprite_->SetColor({ 0.18f, 0.45f, 0.75f, 0.90f });
    pauseRetryBtnSprite_->Update();

    // 4. 「タイトルに戻る」ボタン用枠
    pauseTitleBtnSprite_ = std::make_unique<Sprite>();
    pauseTitleBtnSprite_->Initialize(SpriteManager::GetInstance(), "resources/Textures/white.png");
    pauseTitleBtnSprite_->SetSize({ 220.0f, 44.0f });
    pauseTitleBtnSprite_->SetAnchorPoint({ 0.5f, 0.5f });
    pauseTitleBtnSprite_->SetPosition({ WinApp::GetInstance()->kClientWidth / 2.0f, WinApp::GetInstance()->kClientHeight / 2.0f + 70.0f });
    pauseTitleBtnSprite_->SetColor({ 0.75f, 0.22f, 0.22f, 0.90f });
    pauseTitleBtnSprite_->Update();

    pauseControlBtnSprite_ = std::make_unique<Sprite>();
    pauseControlBtnSprite_->Initialize(SpriteManager::GetInstance(), "resources/Textures/white.png");
    pauseControlBtnSprite_->SetSize({ 300.0f, 44.0f });
    pauseControlBtnSprite_->SetAnchorPoint({ 0.5f, 0.5f });
    pauseControlBtnSprite_->SetPosition({ WinApp::GetInstance()->kClientWidth / 2.0f, WinApp::GetInstance()->kClientHeight / 2.0f + 70.0f });
    pauseControlBtnSprite_->SetColor({ 0.25f, 0.55f, 0.45f, 0.90f });
    pauseControlBtnSprite_->Update();

    pauseTitleBtnSprite_->SetPosition({ WinApp::GetInstance()->kClientWidth / 2.0f, WinApp::GetInstance()->kClientHeight / 2.0f + 190.0f });
    pauseTitleBtnSprite_->Update();

    // -------------------------------------------------
    // ポーズ用日本語テキストUI（Release構成対応 Text描画システム）
    // -------------------------------------------------
    constexpr const char* kDefaultFont = "resources/Fonts/NotoSansJP/NotoSansJP-Variable.ttf";

    pauseTitleText_ = std::make_unique<Text>();
    pauseTitleText_->Initialize(kDefaultFont);
    pauseTitleText_->SetText("PAUSE MENU");
    pauseTitleText_->SetPosition({ WinApp::GetInstance()->kClientWidth / 2.0f, WinApp::GetInstance()->kClientHeight / 2.0f - 135.0f });
    pauseTitleText_->SetAnchorPoint({ 0.5f, 0.5f });
    pauseTitleText_->SetFontSize(34.0f);
    pauseTitleText_->SetColor({ 0.35f, 0.85f, 1.0f, 1.0f });
    pauseTitleText_->Update();

    pauseResumeText_ = std::make_unique<Text>();
    pauseResumeText_->Initialize(kDefaultFont);
    pauseResumeText_->SetText("ゲーム再開 [TAB]");
    pauseResumeText_->SetPosition({ WinApp::GetInstance()->kClientWidth / 2.0f, WinApp::GetInstance()->kClientHeight / 2.0f - 70.0f });
    pauseResumeText_->SetAnchorPoint({ 0.5f, 0.5f });
    pauseResumeText_->SetFontSize(22.0f);
    pauseResumeText_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
    pauseResumeText_->Update();

    pauseRetryText_ = std::make_unique<Text>();
    pauseRetryText_->Initialize(kDefaultFont);
    pauseRetryText_->SetText("リトライ [R]");
    pauseRetryText_->SetPosition({ WinApp::GetInstance()->kClientWidth / 2.0f, WinApp::GetInstance()->kClientHeight / 2.0f });
    pauseRetryText_->SetAnchorPoint({ 0.5f, 0.5f });
    pauseRetryText_->SetFontSize(22.0f);
    pauseRetryText_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
    pauseRetryText_->Update();

    pauseTitleBtnText_ = std::make_unique<Text>();
    pauseTitleBtnText_->Initialize(kDefaultFont);
    pauseTitleBtnText_->SetText("タイトルに戻る [T]");
    pauseTitleBtnText_->SetPosition({ WinApp::GetInstance()->kClientWidth / 2.0f, WinApp::GetInstance()->kClientHeight / 2.0f + 70.0f });
    pauseTitleBtnText_->SetAnchorPoint({ 0.5f, 0.5f });
    pauseTitleBtnText_->SetFontSize(22.0f);
    pauseTitleBtnText_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
    pauseTitleBtnText_->Update();

    pauseControlText_ = std::make_unique<Text>();
    pauseControlText_->Initialize(kDefaultFont);
    pauseControlText_->SetPosition({ WinApp::GetInstance()->kClientWidth / 2.0f, WinApp::GetInstance()->kClientHeight / 2.0f + 70.0f });
    pauseControlText_->SetAnchorPoint({ 0.5f, 0.5f });
    pauseControlText_->SetFontSize(19.0f);
    pauseControlText_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });

    pauseTitleBtnText_->SetPosition({ WinApp::GetInstance()->kClientWidth / 2.0f, WinApp::GetInstance()->kClientHeight / 2.0f + 190.0f });
    pauseTitleBtnText_->Update();

    pauseSensitivityText_ = std::make_unique<Text>();
    pauseSensitivityText_->Initialize(kDefaultFont);
    pauseSensitivityText_->SetPosition({ WinApp::GetInstance()->kClientWidth / 2.0f, WinApp::GetInstance()->kClientHeight / 2.0f + 125.0f });
    pauseSensitivityText_->SetAnchorPoint({ 0.5f, 0.5f });
    pauseSensitivityText_->SetFontSize(18.0f);
    pauseSensitivityText_->SetColor({ 0.75f, 0.95f, 1.0f, 1.0f });

    // 画面左下に表示する現在武器HUD
    InitializeHudFrame(
        weaponHudFrameSprites_,
        { 20.0f, WinApp::GetInstance()->kClientHeight - 20.0f },
        { 288.0f, 84.0f },
        { 0.0f, 1.0f },
        kPlayerActionCyan);
    weaponHudBgSprite_ = std::make_unique<Sprite>();
    weaponHudBgSprite_->Initialize(SpriteManager::GetInstance(), "resources/Textures/white.png");
    weaponHudBgSprite_->SetSize({ 280.0f, 76.0f });
    weaponHudBgSprite_->SetAnchorPoint({ 0.0f, 1.0f });
    weaponHudBgSprite_->SetPosition({ 24.0f, WinApp::GetInstance()->kClientHeight - 24.0f });
    weaponHudBgSprite_->SetColor(kHudPanel);
    weaponHudBgSprite_->Update();

    weaponHudLabelText_ = std::make_unique<Text>();
    weaponHudLabelText_->Initialize(kDefaultFont);
    weaponHudLabelText_->SetText("WEAPON");
    weaponHudLabelText_->SetPosition({ 40.0f, WinApp::GetInstance()->kClientHeight - 92.0f });
    weaponHudLabelText_->SetFontSize(16.0f);
    weaponHudLabelText_->SetColor(kPlayerActionCyan);
    weaponHudLabelText_->Update();

    weaponHudNameText_ = std::make_unique<Text>();
    weaponHudNameText_->Initialize(kDefaultFont);
    weaponHudNameText_->SetText("Normal");
    weaponHudNameText_->SetPosition({ 40.0f, WinApp::GetInstance()->kClientHeight - 68.0f });
    weaponHudNameText_->SetFontSize(27.0f);
    weaponHudNameText_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
    weaponHudNameText_->SetOutlineWidth(1.0f);
    weaponHudNameText_->Update();

    // -------------------------------------------------
    // 画面右側に表示するプレイヤーHPゲージUIの初期化
    // -------------------------------------------------
    InitializeHudFrame(
        playerHudFrameSprites_,
        { WinApp::GetInstance()->kClientWidth - 18.0f, 8.0f },
        { 268.0f, 70.0f },
        { 1.0f, 0.0f },
        kPlayerActionCyan);
    playerHpBgSprite_ = std::make_unique<Sprite>();
    playerHpBgSprite_->Initialize(SpriteManager::GetInstance(), "resources/Textures/white.png");
    playerHpBgSprite_->SetSize({ 220.0f, 22.0f });
    playerHpBgSprite_->SetAnchorPoint({ 1.0f, 0.0f });
    playerHpBgSprite_->SetPosition({ WinApp::GetInstance()->kClientWidth - 30.0f, 40.0f });
    playerHpBgSprite_->SetColor(kHudPanel);
    playerHpBgSprite_->Update();

    playerHpBarSprite_ = std::make_unique<Sprite>();
    playerHpBarSprite_->Initialize(SpriteManager::GetInstance(), "resources/Textures/white.png");
    playerHpBarSprite_->SetMaterial("resources/Shaders/Sprite/HealthBar");
    playerHpBarSprite_->SetSize({ 220.0f, 22.0f });
    playerHpBarSprite_->SetAnchorPoint({ 1.0f, 0.0f });
    playerHpBarSprite_->SetPosition({ WinApp::GetInstance()->kClientWidth - 30.0f, 40.0f });
    playerHpBarSprite_->SetColor(kPlayerActionCyan);
    playerHpBarSprite_->Update();

    playerHpText_ = std::make_unique<Text>();
    playerHpText_->Initialize(kDefaultFont);
    playerHpText_->SetText("HP 20 / 20");
    playerHpText_->SetPosition({ WinApp::GetInstance()->kClientWidth - 30.0f, 12.0f });
    playerHpText_->SetAnchorPoint({ 1.0f, 0.0f });
    playerHpText_->SetFontSize(20.0f);
    playerHpText_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
    playerHpText_->Update();

    const float bossHudCenterX = WinApp::GetInstance()->kClientWidth / 2.0f;
    const float bossHpBarLeft = bossHudCenterX - 170.0f;
    const float bossHpBarWidth = 340.0f;

    InitializeHudFrame(
        bossHudFrameSprites_,
        { bossHudCenterX, 8.0f },
        { 620.0f, 110.0f },
        { 0.5f, 0.0f },
        kDangerVermilion);

    bossHeadHpBgSprite_ = std::make_unique<Sprite>();
    bossHeadHpBgSprite_->Initialize(SpriteManager::GetInstance(), "resources/Textures/white.png");
    bossHeadHpBgSprite_->SetSize({ bossHpBarWidth, 14.0f });
    bossHeadHpBgSprite_->SetAnchorPoint({ 0.0f, 0.0f });
    bossHeadHpBgSprite_->SetPosition({ bossHpBarLeft, 65.0f });
    bossHeadHpBgSprite_->SetColor(kHudPanel);
    bossHeadHpBgSprite_->Update();

    bossHeadHpBarSprite_ = std::make_unique<Sprite>();
    bossHeadHpBarSprite_->Initialize(SpriteManager::GetInstance(), "resources/Textures/white.png");
    bossHeadHpBarSprite_->SetMaterial("resources/Shaders/Sprite/HealthBar");
    bossHeadHpBarSprite_->SetSize({ bossHpBarWidth, 14.0f });
    bossHeadHpBarSprite_->SetAnchorPoint({ 0.0f, 0.0f });
    bossHeadHpBarSprite_->SetPosition({ bossHpBarLeft, 65.0f });
    bossHeadHpBarSprite_->SetColor(kHudBrass);
    bossHeadHpBarSprite_->Update();

    bossBodyHpBgSprite_ = std::make_unique<Sprite>();
    bossBodyHpBgSprite_->Initialize(SpriteManager::GetInstance(), "resources/Textures/white.png");
    bossBodyHpBgSprite_->SetSize({ bossHpBarWidth, 14.0f });
    bossBodyHpBgSprite_->SetAnchorPoint({ 0.0f, 0.0f });
    bossBodyHpBgSprite_->SetPosition({ bossHpBarLeft, 91.0f });
    bossBodyHpBgSprite_->SetColor(kHudPanel);
    bossBodyHpBgSprite_->Update();

    bossBodyHpBarSprite_ = std::make_unique<Sprite>();
    bossBodyHpBarSprite_->Initialize(SpriteManager::GetInstance(), "resources/Textures/white.png");
    bossBodyHpBarSprite_->SetMaterial("resources/Shaders/Sprite/HealthBar");
    bossBodyHpBarSprite_->SetSize({ bossHpBarWidth, 14.0f });
    bossBodyHpBarSprite_->SetAnchorPoint({ 0.0f, 0.0f });
    bossBodyHpBarSprite_->SetPosition({ bossHpBarLeft, 91.0f });
    bossBodyHpBarSprite_->SetColor(kDangerVermilion);
    bossBodyHpBarSprite_->Update();

    bossNameText_ = std::make_unique<Text>();
    bossNameText_->Initialize(kDefaultFont);
    bossNameText_->SetText(stageSettings_.bossType == "AngerBlock"
        ? "BOSS: ANGER"
        : (stageSettings_.bossType == "IceJellyfish"
            ? "BOSS: ICE JELLYFISH" : "BOSS: FEAR WORM"));
    bossNameText_->SetPosition({ bossHudCenterX, 12.0f });
    bossNameText_->SetAnchorPoint({ 0.5f, 0.0f });
    bossNameText_->SetFontSize(20.0f);
    bossNameText_->SetColor(kDangerVermilion);
    bossNameText_->Update();

    bossHeadHpText_ = std::make_unique<Text>();
    bossHeadHpText_->Initialize(kDefaultFont);
    bossHeadHpText_->SetText(stageSettings_.bossType == "AngerBlock"
        ? "ANGER CORE"
        : (stageSettings_.bossType == "IceJellyfish" ? "CORE HP" : "HEAD CORE"));
    bossHeadHpText_->SetPosition({ bossHpBarLeft - 12.0f, 60.0f });
    bossHeadHpText_->SetAnchorPoint({ 1.0f, 0.0f });
    bossHeadHpText_->SetFontSize(14.0f);
    bossHeadHpText_->SetColor(kHudBrass);
    bossHeadHpText_->Update();

    bossBodyHpText_ = std::make_unique<Text>();
    bossBodyHpText_->Initialize(kDefaultFont);
    bossBodyHpText_->SetText(stageSettings_.bossType == "AngerBlock"
        ? "FISTS"
        : (stageSettings_.bossType == "IceJellyfish" ? "TENTACLE PARTS" : "BODY SHIELD"));
    bossBodyHpText_->SetPosition({ bossHpBarLeft - 12.0f, 86.0f });
    bossBodyHpText_->SetAnchorPoint({ 1.0f, 0.0f });
    bossBodyHpText_->SetFontSize(14.0f);
    bossBodyHpText_->SetColor(kDangerVermilion);
    bossBodyHpText_->Update();

    Logger::Log("GamePlayScene::Initialize: Loading uvChecker texture");
    TextureManager::GetInstance()->LoadTexture("resources/Textures/uvChecker.png");
    Logger::Log("GamePlayScene::Initialize: Allocating skyBox");
    skyBox_ = std::make_unique<SkyBox>();
    Logger::Log("GamePlayScene::Initialize: Initializing skyBox");
    skyBox_->Initialize(DirectXCommon::GetInstance());
    Logger::Log("GamePlayScene::Initialize: Setting skyBox texture");
    skyBox_->SetTexture(stageSettings_.skybox);
    Logger::Log("GamePlayScene::Initialize: skyBox initialization finished");

    // =================================================
    // Playerクラス
    // =================================================
    Logger::Log("GamePlayScene::Initialize: Starting player initialization");
    player_ = std::make_unique<Player>();
    player_->Initialize(playerModel);
    player_->SetCamera(camera_.get());
    player_->SetDebugCameraController(debugCameraController_.get());
    player_->SetControlMode(gControlMode);
    player_->SetMouseSensitivity(gMouseSensitivity);
    lastPlayerHp_ = player_->GetMaxHp();

    Vector3 playerStartPos = { 0.0f, 0.0f, 0.0f };
    Vector3 playerStartRot = { 0.0f, 0.0f, 0.0f };

    LevelDataLoader levelDataLoader;
    LevelData levelData = levelDataLoader.Load(stageSettings_.layoutFile);

    if (!levelData.playerSpawns.empty()) {
        const LevelData::PlayerSpawnData& spawn = levelData.playerSpawns[0];
        playerStartPos = spawn.translation;
        playerStartRot = spawn.rotation;
    }

    player_->SetTranslate(playerStartPos);
    player_->SetRotate(playerStartRot);
    player_->SetRailFrame(playerStartPos, { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f });

    bossController_ = std::make_unique<BossEncounterController>();
    bossController_->Initialize(
        stageSettings_.bossType,
        stageSettings_.bossSpawnDistance,
        stageSettings_.bossPosition,
        fearWormEnemyModel_,
        angerBlockModel_,
        enemyBulletModel_,
        player_.get(),
        camera_.get(),
        rail_.get(),
        stageSettings_.bossRailAutoExtension,
        stageSettings_.bossRailExtensionBuffer);

    Logger::Log("GamePlayScene::Initialize: player initialized successfully");
    if (!stageSettings_.recoveryItemDistances.empty()) {
        stageSettings_.recoveryItemPositions.clear();
        for (float distance : stageSettings_.recoveryItemDistances) {
            stageSettings_.recoveryItemPositions.push_back(
                rail_->GetPositionByDistance(distance));
        }
    }
    InitializeRecoveryItems(recoveryItemModel);
    playerJetHandle_ = EffectManager::GetInstance()->AttachEffect("Jet", player_);
    playerJetSparkHandle_ = EffectManager::GetInstance()->AttachEffect("JetSpark", player_);
    wasPlayerBoosting_ = false;

    CreateLevelObjects(levelData);

    // ペイント弾を撃ってくるエネミーをコース上に5体配置（視認しやすくインクを連射する位置）
    for (size_t i = 0; i < stageSettings_.paintEnemyDistances.size(); ++i) {
        std::unique_ptr<PaintShooterEnemy> paintEnemy = std::make_unique<PaintShooterEnemy>();
        paintEnemy->Initialize(enemyModel_, enemyBulletModel_, player_.get());
        float distance = stageSettings_.paintEnemyDistances[i];
        Vector3 railPosition = rail_->GetPositionByDistance(distance);
        Vector3 forward = CalculateRailForward(distance, railPosition);
        Vector3 right {};
        Vector3 up {};
        CalculateRailBasis(forward, right, up);
        float sideOffset = (i % 2 == 0) ? -8.0f : 8.0f;
        paintEnemy->SetPosition(railPosition + right * sideOffset + up * 2.0f);
        enemies_.push_back(std::move(paintEnemy));
    }

    editorManager_->SetSceneObjectManager(sceneObjectManager_.get());

    // floorの初期化
    if (stageSettings_.floorEnabled && stageId_ == "stage01") {
        oceanSurface_ = std::make_unique<OceanSurface>();
        oceanSurface_->Initialize(
            camera_.get(),
            1000.0f,
            stageSettings_.railLength,
            stageSettings_.floorHeight);
        if (stageId_ == "stage01") {
            waterPillarRenderer_ = std::make_unique<WaterPillarRenderer>();
            waterPillarRenderer_->Initialize(camera_.get());
            InitializeOceanLife();
            InitializeWaterPillars();
        }
    } else if (stageSettings_.floorEnabled) {
        Model* floorModel = ModelManager::GetInstance()->CreatePlane(
            stageSettings_.floorTexture, 100.0f, 360.0f);
        floorObj_ = std::make_unique<Object3d>();
        floorObj_->Initialize(Object3dManager::GetInstance());
        floorObj_->SetModel(floorModel);
        floorObj_->SetTranslate({
            0.0f,
            stageSettings_.floorHeight,
            stageSettings_.railLength * 0.5f });
        floorObj_->SetRotate({ std::numbers::pi_v<float> / 2.0f, 0.0f, 0.0f });
        floorObj_->SetScale({ 1000.0f, stageSettings_.railLength, 1.0f });
        if (stageId_ == "stage03") {
            // The floor uses a white texture; its restrained cracks are drawn in the PS.
            floorObj_->SetColor({ 0.54f, 0.73f, 0.86f, 1.0f });
            floorObj_->SetShadingMode(MaterialShadingMode::Ice);
            floorObj_->SetMaterial("resources/Shaders/Object3D/StageIceFloor");
            floorObj_->GetMaterial()->shininess = 0.0f;
            floorObj_->SetEnableEnvironmentMap(false);
        }
    }

    Logger::Log("GamePlayScene::Initialize: Completed successfully");
#if defined(ENABLE_DEVELOPMENT_TOOLS)
    DevelopmentWebPanel::GetInstance().SetScene(this);
#endif
}

Vector3 GamePlayScene::CalculateRailForward(float distance, const Vector3& railPosition) const
{
    if (rail_ == nullptr) {
        return { 0.0f, 0.0f, 1.0f };
    }

    float previousDistance = distance - railDirectionSampleDistance_;
    if (previousDistance < 0.0f) {
        previousDistance = 0.0f;
    }

    float nextDistance = distance + railDirectionSampleDistance_;
    float totalLength = rail_->GetTotalLength();
    if (nextDistance > totalLength) {
        nextDistance = totalLength;
    }

    Vector3 previousPosition = rail_->GetPositionByDistance(previousDistance);
    Vector3 nextPosition = rail_->GetPositionByDistance(nextDistance);
    Vector3 forward = Normalize(nextPosition - previousPosition);

    if (IsNearlyZero(forward)) {
        forward = Normalize(nextPosition - railPosition);
    }

    if (IsNearlyZero(forward)) {
        forward = Normalize(railPosition - previousPosition);
    }

    if (IsNearlyZero(forward)) {
        forward = { 0.0f, 0.0f, 1.0f };
    }

    return forward;
}

StageBoss* GamePlayScene::GetActiveBoss() const
{
    if (bossController_ == nullptr) {
        return nullptr;
    }
    return bossController_->GetActiveBoss();
}

void GamePlayScene::CalculateRailBasis(const Vector3& forward, Vector3& right, Vector3& up) const
{
    Vector3 normalizedForward = Normalize(forward);
    if (IsNearlyZero(normalizedForward)) {
        normalizedForward = { 0.0f, 0.0f, 1.0f };
    }

    Vector3 referenceUp = { 0.0f, 1.0f, 0.0f };
    right = Normalize(Cross(referenceUp, normalizedForward));

    if (IsNearlyZero(right)) {
        Vector3 referenceForward = { 0.0f, 0.0f, 1.0f };
        right = Normalize(Cross(normalizedForward, referenceForward));
    }

    if (IsNearlyZero(right)) {
        right = { 1.0f, 0.0f, 0.0f };
    }

    up = Normalize(Cross(normalizedForward, right));

    if (IsNearlyZero(up)) {
        up = referenceUp;
    }
}

void GamePlayScene::Update()
{
    // TABキーによるポーズメニュー（Pause Menu）切り替え
    Input* input = Input::GetInstance();
#if defined(_DEBUG) || defined(ENABLE_DEVELOPMENT_TOOLS)
    if (input != nullptr && input->IsKeyTrigger(DIK_F6)) {
        developmentPaused_ = !developmentPaused_;
        stepDevelopmentFrame_ = false;
    }
    if (developmentPaused_ && input != nullptr && input->IsKeyTrigger(DIK_F7)) {
        stepDevelopmentFrame_ = true;
    }
    if (developmentPaused_ && !stepDevelopmentFrame_) {
        debugCameraController_->Update();
        camera_->Update();
        return;
    }
    stepDevelopmentFrame_ = false;
#endif
    if (input != nullptr && input->IsKeyTrigger(DIK_TAB)) {
        isPaused_ = !isPaused_;
    }

    if (isPaused_) {
        SceneManager::GetInstance()->SetCameraShakeStrength(0.0f);
        SceneManager::GetInstance()->RemovePostEffect(PostEffectType::CameraShake);

        // ポーズ中は読みやすさに必要なぼかしと彩度低下だけを適用する。
        SceneManager::GetInstance()->AddPostEffect(PostEffectType::GaussianFilter,PostEffectStage::BeforeParticle);
        SceneManager::GetInstance()->AddPostEffect(PostEffectType::GrayScale,PostEffectStage::BeforeParticle);

        // ポーズテキストオブジェクトの更新
        if (pauseTitleText_) pauseTitleText_->Update();
        if (pauseResumeText_) pauseResumeText_->Update();
        if (pauseRetryText_) pauseRetryText_->Update();
        if (pauseTitleBtnText_) pauseTitleBtnText_->Update();
        if (input != nullptr && input->IsKeyTrigger(DIK_C)) {
            gControlMode = gControlMode == Player::ControlMode::KeyboardAndMouse
                ? Player::ControlMode::StarFox
                : Player::ControlMode::KeyboardAndMouse;
            if (player_) {
                player_->SetControlMode(gControlMode);
            }
        }
        if (pauseControlText_) {
            pauseControlText_->SetText(
                gControlMode == Player::ControlMode::StarFox
                    ? "CONTROL: STARFOX [C]"
                    : "CONTROL: WASD + MOUSE [C]");
            pauseControlText_->Update();
        }
        if (input != nullptr && input->IsKeyTrigger(DIK_LBRACKET)) {
            gMouseSensitivity = std::clamp(
                gMouseSensitivity - 0.1f, 0.5f, 2.0f);
            if (player_) player_->SetMouseSensitivity(gMouseSensitivity);
        }
        if (input != nullptr && input->IsKeyTrigger(DIK_RBRACKET)) {
            gMouseSensitivity = std::clamp(
                gMouseSensitivity + 0.1f, 0.5f, 2.0f);
            if (player_) player_->SetMouseSensitivity(gMouseSensitivity);
        }
        if (pauseSensitivityText_) {
            int sensitivityPercent = static_cast<int>(gMouseSensitivity * 100.0f + 0.5f);pauseSensitivityText_->SetText("MOUSE SENSITIVITY: " +std::to_string(sensitivityPercent) +"%  [[ / ]] ");
            pauseSensitivityText_->Update();
        }

        // Tキーでタイトル画面へ戻る
        if (input != nullptr && input->IsKeyTrigger(DIK_T)) {
            ResetGameplayPostEffects();
            SceneManager::GetInstance()->SetNextScene(std::make_unique<TitleScene>());
            return;
        }

        // Rキーでステージリトライ
        if (input != nullptr && input->IsKeyTrigger(DIK_R)) {
            ResetGameplayPostEffects();
            SceneManager::GetInstance()->SetNextScene(
                std::make_unique<GamePlayScene>(stageId_));
            return;
        }

        // ポーズ中はゲームオブジェクトの更新を停止
        debugCameraController_->Update();
        camera_->Update();
        return;
    }
    if (Input::GetInstance()->IsKeyTrigger(DIK_F5) || Input::GetInstance()->IsKeyTrigger(DIK_R)) {
        HotReloadLevel();
    }

    // 時間停止中はゲーム世界を更新しない。
    // チュートリアルUIは、今後この判定より前でUnscaledDeltaTimeを使って更新する。
    if (TimeManager::GetInstance()->GetDeltaTime() <= 0.0f) {
        debugCameraController_->Update();
        camera_->Update();
        return;
    }

    // HPが尽きた後は通常のゲーム進行を止め、落下と爆発だけを更新する。
    if (player_ && player_->IsDead()) {
        StopPlayerEngineEffects();
        player_->Update();

        // 落下するPlayerとの距離を保ちながら、カメラも滑らかに追従する。
        if (camera_) {
            const Vector3 playerPosition = player_->GetTranslate();
            if (!hasPlayerDeathCameraState_) {
                hasPlayerDeathCameraState_ = true;
                playerDeathCameraOffset_ =
                    camera_->GetTranslate() - playerPosition;
                playerDeathCameraLookTarget_ = smoothedLookAheadPosition_;
            }

            const Vector3 targetCameraPosition =
                playerPosition + playerDeathCameraOffset_;
            const Vector3 cameraPosition = Lerp(
                camera_->GetTranslate(),
                targetCameraPosition,
                0.15f);
            playerDeathCameraLookTarget_ = Lerp(
                playerDeathCameraLookTarget_,
                playerPosition,
                0.15f);
            camera_->LookAt(
                cameraPosition,
                playerDeathCameraLookTarget_);
            camera_->Update();
        }

        if (player_->IsDeathExplosionReady()) {
            if (!playerDeathExplosionPlayed_) {
                playerDeathExplosionPlayed_ = true;
                EffectManager::GetInstance()->PlayEffect(
                    "Explosion",
                    player_->GetTranslate());
                cameraShakeTime_ = 0.45f;
                cameraShakeDuration_ = 0.45f;
                cameraShakeStrength_ = 0.018f;
            }

            playerDeathAfterExplosionTimer_ +=
                TimeManager::GetInstance()->GetDeltaTime();
            if (playerDeathAfterExplosionTimer_ >= 0.8f) {
                ResetGameplayPostEffects();
                SceneManager::GetInstance()->SetNextScene(
                    std::make_unique<GameOverScene>(stageId_));
                return;
            }
        }

        EffectManager::GetInstance()->Update();
        UpdateCameraShakePostEffect();
        return;
    }
    // ReleaseビルドでもVキーでボス戦の開始位置へワープできる。
    if (stageSettings_.bossType != "None" && Input::GetInstance()->IsKeyTrigger(DIK_V)) {
        railDistance_ = (std::max)(0.0f, stageSettings_.bossSpawnDistance);
        if (player_) {
            player_->SetTranslate(rail_->GetPositionByDistance(railDistance_));
        }
    }
    // レール自体の更新
    rail_->Update();

    // 敵全体の更新
    for (std::unique_ptr<BaseEnemy>& enemy : enemies_) {
        enemy->Update();
    }

    std::erase_if(enemies_, [](const std::unique_ptr<BaseEnemy>& enemy) {
        return enemy->IsDead();
    });
    const Vector3 currentRailPosition =
        rail_->GetPositionByDistance(railDistance_);
    const Vector3 currentRailForward =
        CalculateRailForward(railDistance_, currentRailPosition);
    enemyBulletManager_.Update(
        player_->GetTranslate(),
        currentRailForward);

    float pirateShipSpawnDistance = -1.0f;
    float pirateShipPositionDistance = 0.0f;
    if (stageId_ == "stage03" && stageSettings_.bossType != "None") {
        pirateShipSpawnDistance = 1250.0f;
        pirateShipPositionDistance = 1340.0f;
    }

    if (pirateShipSpawnDistance >= 0.0f &&
        !isPirateShipMidBossSpawned_ &&
        railDistance_ >= pirateShipSpawnDistance) {
        auto pirateShip = std::make_unique<PirateShipMidBoss>();
        pirateShip->Initialize(angerBlockModel_, enemyBulletModel_, player_.get());
        Vector3 spawnPosition = rail_->GetPositionByDistance(pirateShipPositionDistance);
        spawnPosition.y = stageSettings_.floorHeight;
        pirateShip->SetPosition(spawnPosition);
        enemies_.push_back(std::move(pirateShip));
        isPirateShipMidBossSpawned_ = true;
    }

    // プレイヤーのZ座標を取得
    UpdateSwarmWaveSpawning();

    // ボス出現処理
    bossController_->Update(railDistance_);
    if (bossController_->DidExtendRailThisFrame() && oceanSurface_ != nullptr) {
        oceanSurface_->SetLength(rail_->GetTotalLength());
    }
    if (bossController_->DidExtendRailThisFrame() && floorObj_ != nullptr) {
        const float floorLength = rail_->GetTotalLength();
        floorObj_->SetTranslate({ 0.0f, stageSettings_.floorHeight, floorLength * 0.5f });
        floorObj_->SetScale({ 1000.0f, floorLength, 1.0f });
    }

    // プレイヤーのHP減少検知による被弾カメラシェイク
    if (player_) {
        static int lastPlayerHp = player_->GetCurrentHp();
        int currentPlayerHp = player_->GetCurrentHp();
        if (currentPlayerHp < lastPlayerHp) {
            cameraShakeTime_ = kPlayerDamageShakeDuration;
            cameraShakeDuration_ = kPlayerDamageShakeDuration;
            cameraShakeStrength_ = kPlayerDamageShakeStrength;
        }
        lastPlayerHp = currentPlayerHp;
    }

    // ボスの更新
    StageBoss* activeBoss = GetActiveBoss();
    if (activeBoss != nullptr) {
        UpdateBossHpHud();

        // 発狂モードに入った瞬間を検知してカメラシェイクを開始する
        if (bossController_->DidEnterMadModeThisFrame()) {
            cameraShakeTime_ = kBossMadShakeDuration;
            cameraShakeDuration_ = kBossMadShakeDuration;
            cameraShakeStrength_ = kBossMadShakeStrength;
        }

        // ビーム被弾中のカメラ微振動
        if (bossController_->IsBeamHittingPlayer()) {
            if (cameraShakeTime_ < kBossBeamShakeDuration ||
                cameraShakeStrength_ < kBossBeamShakeStrength) {
                cameraShakeTime_ = kBossBeamShakeDuration;
                cameraShakeDuration_ = kBossBeamShakeDuration;
                cameraShakeStrength_ = kBossBeamShakeStrength;
            }
        }
    }

    // ボス撃破でディゾルブ消滅演出の完了後にクリアシーンへ遷移
    if (activeBoss != nullptr && activeBoss->IsDead()) {
        cameraShakeTime_ = 0.0f;
        cameraShakeDuration_ = 0.0f;
        cameraShakeStrength_ = 0.0f;
        SceneManager::GetInstance()->SetCameraShakeStrength(0.0f);
        SceneManager::GetInstance()->RemovePostEffect(PostEffectType::CameraShake);

        // 死亡演出(頭部の落下回転)が完了するまでボスのUpdateを回し続ける
        bossController_->UpdateDeathSequence();
        EffectManager::GetInstance()->Update();

        if (activeBoss->IsDeathSequenceFinished()) {
            StopPlayerEngineEffects();
            bossDeathDissolveTimer_ += TimeManager::GetInstance()->GetDeltaTime();
            float dissolveProgress = bossDeathDissolveTimer_ / 2.0f;
            if (dissolveProgress > 1.0f) dissolveProgress = 1.0f;

            // 撃破ディゾルブポストエフェクトの適用
            SceneManager::GetInstance()->SetVignetteStrength(dissolveProgress);
            SceneManager::GetInstance()->AddPostEffect(
                PostEffectType::Dissolve,
                PostEffectStage::BeforeParticle);

            // たっぷり2.0秒かけてディゾルブ消滅が100%完了してからクリア画面へ遷移！
            if (dissolveProgress >= 1.0f) {
                ResetGameplayPostEffects();
                SceneManager::GetInstance()->SetNextScene(std::make_unique<ClearScene>());
            }
        }

        return;
    }

    // エディターマネージャーの更新にカメラを渡す
    editorManager_->Update(camera_.get());

    // 1. レールの移動座標・方向ベクトルの計算
    Vector3 currentPosition {};
    Vector3 forward {};
    Vector3 railRight {};
    Vector3 railUp {};
    float nextRailDistance = 0.0f;
    UpdateRailMovement(currentPosition, forward, railRight, railUp, nextRailDistance);

    // 静的フラグ（初回フレームのログ出力用）
    static bool isFirstFrame = true;
#ifdef _DEBUG
    if (isFirstFrame) {
        Logger::Log("GamePlayScene::Update: First frame start");
    }
#endif

    gameplayCollisionSystem_->SyncRaycastTargets(
        enemies_,
        GetActiveBoss());

    // 2. プレイヤーの位置・回転などのワールドトランスフォームの確定
    UpdatePlayerTransform(currentPosition, railRight, railUp, forward);
    const bool isPlayerBoosting = player_->IsBoosting();
    UpdateBoostKick(isPlayerBoosting);

    // 進行距離を更新
    railDistance_ = nextRailDistance;

    // 3. 描画用カメラと仮想カメラの同期・更新
    UpdateCamera(currentPosition, forward, railRight, railUp, nextRailDistance, input);
    UpdateOceanLife(currentPosition, forward, railRight);
    // 4. マウス左クリックによる弾の発射処理
    ProcessPlayerShooting(input);

#ifdef _DEBUG
    if (isFirstFrame) {
        Logger::Log("GamePlayScene::Update: First frame completed successfully");
        isFirstFrame = false;
    }
#endif

    // デバッグ用の進行方向ライン描画 (緑色)
#ifdef _DEBUG
    DebugRenderer::GetInstance()->AddLine(
        currentPosition,
        currentPosition + forward * 20.0f,
        { 0.0f, 1.0f, 0.0f, 1.0f },
        3.0f);
#endif

    // プレイヤーのブースト状態に応じたエフェクト制御
    if (isPlayerBoosting != wasPlayerBoosting_) {
        EffectManager::GetInstance()->StopEffect(playerJetHandle_);
        EffectManager::GetInstance()->StopEffect(playerJetSparkHandle_);

        const char* jetEffectName = "Jet";
        const char* sparkEffectName = "JetSpark";
        if (isPlayerBoosting) {
            jetEffectName = "JetBoost";
            sparkEffectName = "JetBoostSpark";
        }
        playerJetHandle_ = EffectManager::GetInstance()->AttachEffect(jetEffectName, player_);
        playerJetSparkHandle_ = EffectManager::GetInstance()->AttachEffect(sparkEffectName, player_);

        wasPlayerBoosting_ = isPlayerBoosting;
    }

    UpdateBoostPostEffectCenter(nextRailDistance, isPlayerBoosting);

    ConfigureGameplayPostEffects(isPlayerBoosting);

    // -------------------------------------------------
    // ブースト加速トリガー時の「衝撃音波グラデーション (SonicBoom)」演出
    // -------------------------------------------------
    static bool prevBoostingState = false;
    bool isShiftPressed = Input::GetInstance()->IsKeyTrigger(DIK_LSHIFT) || Input::GetInstance()->IsKeyTrigger(DIK_RSHIFT);

    // シフトキーを押した瞬間、またはブースト未開始から開始に切り替わった瞬間に100%確実に発動！
    if (isShiftPressed || (isPlayerBoosting && !prevBoostingState)) {
        sonicBoomTimer_ = 0.85f;

        // プレイヤーの現在位置を中心発生源として画面UV座標(0.0〜1.0)へ変換
        if (player_ && camera_) {
            Vector3 playerWorldPos = player_->GetTranslate();
            Vector2 screenPos = camera_->WorldToScreen(playerWorldPos);
            float screenW = static_cast<float>(WinApp::GetInstance()->GetClientWidth());
            float screenH = static_cast<float>(WinApp::GetInstance()->GetClientHeight());
            Vector2 playerUV = { screenPos.x / screenW, screenPos.y / screenH };
            SceneManager::GetInstance()->SetSonicBoomCenter(playerUV);
        }
    }
    prevBoostingState = isPlayerBoosting;

    if (sonicBoomTimer_ > 0.0f) {
        sonicBoomTimer_ -= TimeManager::GetInstance()->GetDeltaTime();
        if (sonicBoomTimer_ < 0.0f) sonicBoomTimer_ = 0.0f;

        float boomProgress = 1.0f - (sonicBoomTimer_ / 0.85f);
        SceneManager::GetInstance()->SetSonicBoomProgress(boomProgress);
        SceneManager::GetInstance()->AddPostEffect(
            PostEffectType::SonicBoom,
            PostEffectStage::BeforeParticle);
    }

    // ペイントポストエフェクトのタイマー更新（時間経過で垂れて落ちる）
    // ★加点要素: BoxFilter (3点) をインク付着時の油分視界ぼやけとして同時適用し、時間経過で徐々に減衰フェードアウト！
    if (isPaintEffectActive_) {
        paintEffectTimer_ += TimeManager::GetInstance()->GetDeltaTime();
        float progress = paintEffectTimer_ / paintEffectDuration_;
        if (progress >= 1.0f) {
            isPaintEffectActive_ = false;
            paintEffectTimer_ = 0.0f;
            SceneManager::GetInstance()->RemovePostEffect(PostEffectType::Paint);
            SceneManager::GetInstance()->RemovePostEffect(PostEffectType::smoothing);
            SceneManager::GetInstance()->SetPaintProgress(0.0f);
            SceneManager::GetInstance()->SetPaintIntensity(0.0f);
        } else {
            // 時間経過に伴い 1.0f -> 0.0f へ徐々にフェードアウトする BoxFilter ブラー強度
            float boxFilterFade = 1.0f - progress;
            SceneManager::GetInstance()->SetVignetteStrength(boxFilterFade);

            SceneManager::GetInstance()->AddPostEffect(
                PostEffectType::Paint,
                PostEffectStage::AfterParticle);
            SceneManager::GetInstance()->AddPostEffect(
                PostEffectType::smoothing,
                PostEffectStage::AfterParticle);
            SceneManager::GetInstance()->SetPaintProgress(progress);
            SceneManager::GetInstance()->SetPaintIntensity(1.0f);
        }
    }

    UpdateWaterDropEffect();

    // -------------------------------------------------
    // 加点要素: Vignetting (3点)
    // 1. ダメージを受けた瞬間は一瞬だけ暗く赤くフラッシュ
    // 2. HPが3以下になったら常時ドクンドクンと脈動（鼓動パルス）
    // -------------------------------------------------
    if (player_) {
        int currentHp = player_->GetCurrentHp();

        // ダメージ検知
        if (currentHp < lastPlayerHp_) {
            damageFlashTimer_ = 0.35f;
        }
        lastPlayerHp_ = currentHp;

        // ダメージフラッシュタイマー消化
        if (damageFlashTimer_ > 0.0f) {
            damageFlashTimer_ -= TimeManager::GetInstance()->GetDeltaTime();
            if (damageFlashTimer_ < 0.0f) damageFlashTimer_ = 0.0f;
        }

        // (A) 被弾瞬間の一瞬暗赤色フラッシュ (小さめでスタイリッシュな範囲)
        if (damageFlashTimer_ > 0.0f) {
            float flashRatio = damageFlashTimer_ / 0.35f;
            SceneManager::GetInstance()->SetVignetteStrength(0.35f + 0.35f * flashRatio);
            SceneManager::GetInstance()->AddPostEffect(
                PostEffectType::Vignette,
                PostEffectStage::BeforeParticle);
        }
        // (B) HP ≦ 3 時の常時ドクンドクン脈動演出 (小さめの四隅赤色鼓動)
        else if (currentHp <= 3) {
            static float vignettePulseTimer = 0.0f;
            vignettePulseTimer += TimeManager::GetInstance()->GetDeltaTime();

            float pulseFactor = 0.45f + 0.25f * std::sin(vignettePulseTimer * 8.5f);
            SceneManager::GetInstance()->SetVignetteStrength(pulseFactor);
            SceneManager::GetInstance()->AddPostEffect(
                PostEffectType::Vignette,
                PostEffectStage::BeforeParticle);
        }
    }

    // -------------------------------------------------
    // 画面右側のプレイヤーHPゲージのリアルタイム更新
    // -------------------------------------------------
    if (player_ && playerHpBarSprite_ && playerHpText_) {
        int currentHp = player_->GetCurrentHp();
        int maxHp = player_->GetMaxHp();
        float hpRatio = 0.0f;
        if (maxHp > 0) {
            hpRatio = static_cast<float>(currentHp) / static_cast<float>(maxHp);
        }
        if (hpRatio < 0.0f) hpRatio = 0.0f;
        if (hpRatio > 1.0f) hpRatio = 1.0f;

        // 残りHP割合に合わせてゲージの横幅を滑らかに変更
        displayedPlayerHpRatio_ = std::lerp(
            displayedPlayerHpRatio_,
            hpRatio,
            0.12f);
        if (std::abs(displayedPlayerHpRatio_ - hpRatio) < 0.001f) {
            displayedPlayerHpRatio_ = hpRatio;
        }

        float barWidth = 220.0f * displayedPlayerHpRatio_;
        playerHpBarSprite_->SetSize({ barWidth, 22.0f });

        // 回復色の緑はアイテム専用。通常HPはプレイヤー色のシアンで示す。
        if (currentHp <= 3) {
            playerHpBarSprite_->SetColor(kDangerVermilion);
        } else if (hpRatio < 0.45f) {
            playerHpBarSprite_->SetColor(kHudBrass);
        } else {
            playerHpBarSprite_->SetColor(kPlayerActionCyan);
        }

        playerHpBarSprite_->Update();
        if (playerHpBgSprite_) playerHpBgSprite_->Update();

        // HP数値テキストの更新
        playerHpText_->SetText("HP " + std::to_string(currentHp) + " / " + std::to_string(maxHp));
        playerHpText_->Update();
    }

    // レティクル（AimSprite）のスクリーン位置更新
    aimSprite_->SetPosition(player_->GetAimScreenPosition());
    aimSprite_->Update();
    std::vector<Vector3> homingLockPositions;
    player_->GetHomingLockPositions(homingLockPositions);
    const size_t markerCount =
        (std::min)(homingLockPositions.size(), homingLockSprites_.size());
    for (size_t index = 0; index < markerCount; ++index) {
        homingLockSprites_[index]->SetPosition(
            camera_->WorldToScreen(homingLockPositions[index]));
        homingLockSprites_[index]->Update();
    }

    // スカイボックスの更新
    skyBox_->Update(camera_.get());

    // 各種マネージャー、オブジェクト、コリジョンの更新
    EffectManager::GetInstance()->Update();
    sceneObjectManager_->Update();
    for (std::unique_ptr<Object3d>& levelObject : levelObjects_) {
        levelObject->Update();
    }
    if (isFishSchoolActive_) {
        for (std::unique_ptr<Object3d>& fish : oceanFish_) fish->Update();
    }
    for (std::unique_ptr<Object3d>& bird : oceanBirds_) bird->Update();
    if (waterPillarRenderer_) {
        waterPillarRenderer_->Update(TimeManager::GetInstance()->GetDeltaTime());
    }
    for (std::unique_ptr<WaterPillarHazard>& pillar : waterPillars_) {
        pillar->Update(railDistance_, TimeManager::GetInstance()->GetDeltaTime());
        if (pillar->CheckCollision(player_->GetTranslate())) {
            if (player_->ApplyDamage(2)) {
                StartWaterDropEffect();
            }
        }
    }
    UpdateRecoveryItems();
    if (floorObj_) {
        floorObj_->Update();
    }
    if (oceanSurface_) {
        oceanSurface_->Update(TimeManager::GetInstance()->GetDeltaTime());
    }

    animationActor_->Update(TimeManager::GetInstance()->GetDeltaTime());
    
    // コリジョン判定の実行
    CheckCollision();
    UpdateCameraShakePostEffect();

#pragma region
#if defined(ENABLE_DEVELOPMENT_TOOLS)
    ApplyDevelopmentLighting();
#elif defined(USE_IMGUI)

    // player_->DrawImGui();

    // ==================================
    // Lighting Panel（ライト操作パネル）
    // ==================================
    ImGui::Begin("Lighting Control");

    // ---- ライトの ON / OFF ----
    bool& lightEnabled = lightEnabled_;
    ImGui::Checkbox("Enable Light", &lightEnabled);

    // ---- ライトの色 ----
    Vector4& lightColor = lightColor_;
    ImGui::ColorEdit3("Light Color", (float*)&lightColor);

    // ---- 明るさ（強さ） ----
    float& lightIntensity = lightIntensity_;
    ImGui::SliderFloat("Intensity", &lightIntensity, 0.0f, 5.0f);

    // ---- 光の向き ----
    Vector3& lightDir = lightDir_;
    ImGui::SliderFloat3("Direction", &lightDir.x, -1.0f, 1.0f);

    // ---- 正規化 ----
    Vector3 normalizedDir = Normalize(lightDir);

    float intensity = lightIntensity;
    if (!lightEnabled) {
        intensity = 0.0f; // OFF のときは光なし
    }

    LightManager::GetInstance()->SetDirectional(
        { lightColor.x, lightColor.y, lightColor.z, 1.0f },
        normalizedDir,
        intensity);

    Vector4& ambientColor = ambientColor_;

    // ---- リセットボタン（向きだけ元に戻す）---
    if (ImGui::Button("Reset Direction")) {
        lightDir = { -0.35f, -0.82f, 0.45f };
    }

    ImGui::SameLine();

    // ---- ライトを完全初期化 ----
    if (ImGui::Button("Reset Light")) {
        lightEnabled = true;
        lightColor = Vector4(0.90f, 0.96f, 1.0f, 1.0f);
        lightIntensity = 0.90f;
        lightDir = { -0.35f, -0.82f, 0.45f };
        ambientColor = Vector4(0.40f, 0.52f, 0.68f, 0.24f);
    }

    ImGui::ColorEdit3("Ambient Color", &ambientColor.x);
    ImGui::SliderFloat("Ambient Intensity", &ambientColor.w, 0.0f, 1.0f);
    LightManager::GetInstance()->SetAmbientColor({ ambientColor.x, ambientColor.y, ambientColor.z });
    LightManager::GetInstance()->SetAmbientIntensity(ambientColor.w);

    ImGui::End();

    // Point Light コントロール
    ImGui::Begin("Point Light Control");
    bool& pointEnabled = pointEnabled_;
    ImGui::Checkbox("Enable Point Light", &pointEnabled);

    Vector4& pointColor = pointColor_;
    ImGui::ColorEdit3("Point Color", (float*)&pointColor);

    Vector3& pointPos = pointPos_;
    ImGui::SliderFloat3("Point Position", &pointPos.x, -10.0f, 10.0f);

    float& pointIntensity = pointIntensity_;
    ImGui::SliderFloat("Point Intensity", &pointIntensity, 0.0f, 5.0f);

    float& pointRadius = pointRadius_;
    float& pointDecay = pointDecay_;
    ImGui::SliderFloat("Point Radius", &pointRadius, 0.1f, 30.0f);
    ImGui::SliderFloat("Point Decay", &pointDecay, 0.1f, 5.0f);

    float pI = 0.0f;
    if (pointEnabled) {
        pI = pointIntensity;
    }
    LightManager::GetInstance()->SetPointRadius(pointRadius);
    LightManager::GetInstance()->SetPointDecay(pointDecay);
    LightManager::GetInstance()->SetPointLight(pointColor, pointPos, pI);

    ImGui::End();

    // Spot Light コントロール
    ImGui::Begin("Spot Light Control");
    bool& spotEnabled = spotEnabled_;
    ImGui::Checkbox("Enable Spot Light", &spotEnabled);

    // 色
    Vector4& spotColor = spotColor_;
    ImGui::ColorEdit3("Spot Color", (float*)&spotColor);

    // 位置
    Vector3& spotPos = spotPos_;
    ImGui::SliderFloat3("Spot Position", &spotPos.x, -10.0f, 10.0f);

    // 方向
    Vector3& spotDir = spotDir_;
    ImGui::SliderFloat3("Spot Direction", &spotDir.x, -1.0f, 1.0f);
    Vector3 normalizedSpotDir = Normalize(spotDir);

    // 強さ
    float& spotIntensity = spotIntensity_;
    ImGui::SliderFloat("Spot Intensity", &spotIntensity, 0.0f, 10.0f);

    // 距離・減衰
    float& spotDistance = spotDistance_;
    float& spotDecay = spotDecay_;
    ImGui::SliderFloat("Spot Distance", &spotDistance, 0.1f, 30.0f);
    ImGui::SliderFloat("Spot Decay", &spotDecay, 0.1f, 5.0f);

    // 角度（度数で操作し、cos に変換）
    float& spotAngleDeg = spotAngleDeg_;
    float& spotFalloffStartDeg = spotFalloffStartDeg_;

    ImGui::SliderFloat("Spot Angle (deg)", &spotAngleDeg, 2.0f, 90.0f);
    ImGui::SliderFloat("Falloff Start (deg)", &spotFalloffStartDeg, 1.0f, spotAngleDeg - 1.0f);

    // cos に変換
    float cosAngle = std::cos(spotAngleDeg * std::numbers::pi_v<float> / 180.0f);
    float cosFalloffStart = std::cos(spotFalloffStartDeg * std::numbers::pi_v<float> / 180.0f);

    // OFF のとき
    float sI = 0.0f;
    if (spotEnabled) {
        sI = spotIntensity;
    }

    // LightManager に反映
    auto* lm = LightManager::GetInstance();
    lm->SetSpotLightColor(spotColor);
    lm->SetSpotLightPosition(spotPos);
    lm->SetSpotLightDirection(normalizedSpotDir);
    lm->SetSpotLightIntensity(sI);
    lm->SetSpotLightDistance(spotDistance);
    lm->SetSpotLightDecay(spotDecay);
    lm->SetSpotLightCosAngle(cosAngle);
    lm->SetSpotLightCosFalloffStart(cosFalloffStart);

    ImGui::End();

    // 反映
#else
    ApplyStageVisualPreset();
#endif // USE_IMGUI

    // terrain_->SetTranslate(terrainPos);
    // terrain_->SetRotate(terrainRotate);
    // terrain_->SetScale(terrainScale);
#pragma endregion
}

void GamePlayScene::UpdateRailMovement(
    Vector3& outPosition,
    Vector3& outForward,
    Vector3& outRight,
    Vector3& outUp,
    float& outNextDistance)
{
    // 次フレームのレール上の進行距離を計算
    const float frameScale = TimeManager::GetInstance()->GetDeltaTime() * 60.0f;
    outNextDistance = railDistance_ + railSpeed_ * frameScale;
    if (outNextDistance > rail_->GetTotalLength()) {
        outNextDistance = rail_->GetTotalLength();
    }

    // レール上での次の座標と前方向（Forward）ベクトルを算出
    outPosition = rail_->GetPositionByDistance(outNextDistance);
    outForward = CalculateRailForward(outNextDistance, outPosition);

    // 前方向ベクトルを基準に、レールの右方向（Right）と上方向（Up）の軸を計算
    CalculateRailBasis(outForward, outRight, outUp);
}

void GamePlayScene::UpdatePlayerTransform(
    const Vector3& currentPosition,
    const Vector3& railRight,
    const Vector3& railUp,
    const Vector3& forward)
{
    std::vector<BaseEnemy*> homingTargets;
    homingTargets.reserve(enemies_.size() + 1);
    for (const std::unique_ptr<BaseEnemy>& enemy : enemies_) {
        if (!enemy->IsDead()) {
            homingTargets.push_back(enemy.get());
        }
    }
    if (BaseEnemy* boss = GetActiveBoss(); boss != nullptr && !boss->IsDead()) {
        homingTargets.push_back(boss);
    }
    player_->SetHomingTargets(homingTargets);

    // プレイヤーにレール情報の最新のフレーム（座標、右方向、上方向、前方向）を伝える
    player_->SetRailFrame(currentPosition, railRight, railUp, forward);
    
    // プレイヤーの内部座標（移動制限など）を更新
    player_->Update();
    if (weaponHudNameText_) {
        weaponHudNameText_->SetText(player_->GetCurrentWeaponDisplayName());
        weaponHudNameText_->Update();
    }

    // 進行方向に合わせてプレイヤーの回転を適用
    if (forward.x != 0.0f || forward.y != 0.0f || forward.z != 0.0f) {
        float horizontalLength = std::sqrt(forward.x * forward.x + forward.z * forward.z);
        Vector3 playerRotate {};
        playerRotate.x = -std::atan2(forward.y, horizontalLength);
        playerRotate.y = -std::atan2(forward.x, forward.z);
        playerRotate.z = 0.0f;

        if (player_->GetControlMode() == Player::ControlMode::StarFox) {
            const float screenWidth = static_cast<float>(
                WinApp::GetInstance()->GetClientWidth());
            const float screenHeight = static_cast<float>(
                WinApp::GetInstance()->GetClientHeight());
            if (screenWidth > 0.0f && screenHeight > 0.0f) {
                const Vector2& steering =
                    player_->GetStarFoxSteeringInput();
                float aimX = steering.x;
                float aimY = steering.y;

                // Point the nose toward the reticle and bank into horizontal
                // movement, while preserving the rail's base orientation.
                constexpr float kMaxAimYaw = 0.42f;
                constexpr float kMaxAimPitch = 0.34f;
                constexpr float kMaxAimBank = 0.30f;
                playerRotate.y -= aimX * kMaxAimYaw;
                playerRotate.x += aimY * kMaxAimPitch;
                playerRotate.z = -aimX * kMaxAimBank;
            }
        }
        player_->SetRotate(playerRotate);
    }

    // プレイヤーのキーボード移動オフセットを考慮した最新のワールド座標を確定・適用
    Vector3 railOffset = player_->GetRailOffset();
    Vector3 playerPosition = currentPosition;
    playerPosition += railRight * railOffset.x;
    playerPosition += railUp * railOffset.y;
    player_->SetTranslate(playerPosition);
}

void GamePlayScene::UpdateCamera(
    const Vector3& currentPosition,
    const Vector3& forward,
    const Vector3& railRight,
    const Vector3& railUp,
    float nextRailDistance,
    Input* input)
{
    debugCameraController_->Update();
    // カメラポイント補間の適用
    if (hasCameraPoint_ && !debugCameraController_->GetDebugMode()) {
        float deltaTime = TimeManager::GetInstance()->GetDeltaTime();
        cameraPointLerpTime_ += deltaTime;
        float moveTime = cameraPointObject_.cameraPoint.moveTime;
        if (moveTime <= 0.0f) {
            moveTime = 1.0f;
        }
        float t = cameraPointLerpTime_ / moveTime;
        if (t > 1.0f) {
            t = 1.0f;
        }

        // カメラ位置の補間
        Vector3 currentEye = camera_->GetTranslate();
        Vector3 targetEye = cameraPointObject_.translation;
        Vector3 eye = {
            currentEye.x + (targetEye.x - currentEye.x) * t,
            currentEye.y + (targetEye.y - currentEye.y) * t,
            currentEye.z + (targetEye.z - currentEye.z) * t
        };

        // カメラ注視点の補間
        Vector3 currentTarget = smoothedLookAheadPosition_;
        Vector3 targetTarget = cameraPointObject_.cameraPoint.target;
        Vector3 target = {
            currentTarget.x + (targetTarget.x - currentTarget.x) * t,
            currentTarget.y + (targetTarget.y - currentTarget.y) * t,
            currentTarget.z + (targetTarget.z - currentTarget.z) * t
        };

        camera_->LookAt(eye, target);
        camera_->Update();
        return;
    }

    // ブースト中かどうかで視野角（FOV）を切り替える
    bool isBoostingForCamera = false;
    if (input != nullptr) {
        isBoostingForCamera = input->IsKeyPressed(DIK_LSHIFT);
    }

    float targetFovY = normalFovY_;
    if (isBoostingForCamera) {
        targetFovY = boostFovY_;
    }

    // FOVの補間計算とカメラへの適用
    currentFovY_ += (targetFovY - currentFovY_) * fovLerpRate_;
    camera_->SetFovY(currentFovY_);

    // デバッグモードでない場合は、描画用カメラをレールに沿って遅延追従（Lerp）させる
    if (!debugCameraController_->GetDebugMode()) {
        Vector3 cameraForward = forward;

        if (hasCameraFollowState_) {
            Vector3 lerpedForward = Lerp(smoothedCameraForward_, forward, cameraForwardLerpRate_);
            if (!IsNearlyZero(lerpedForward)) {
                cameraForward = Normalize(lerpedForward);
            }
        }

        smoothedCameraForward_ = cameraForward;

        // 描画用カメラのターゲット座標（Lerp前）
        // プレイヤーのレール相対移動量を取得
        Vector3 playerRailOffset = player_->GetRailOffset();

        Vector3 targetDrawCameraPosition = currentPosition - cameraForward * kCameraBackwardOffset;
        targetDrawCameraPosition += railUp * (kCameraUpwardOffset + playerRailOffset.y * cameraHeightFollowFactor_);
        targetDrawCameraPosition += railRight *
            (playerRailOffset.x * cameraHorizontalFollowFactor_);

        // 描画用カメラのターゲット注視点（Lerp前）
        Vector3 targetLookAheadPositionDraw = rail_->GetPositionByDistance(nextRailDistance + cameraLookAheadDistance_);
        targetLookAheadPositionDraw += railUp * (playerRailOffset.y * cameraLookUpFactor_);
        targetLookAheadPositionDraw += railRight *
            (playerRailOffset.x * cameraLookHorizontalFactor_);

        // 遅延追従（Lerp）の適用
        if (hasCameraFollowState_) {
            smoothedCameraPosition_ = Lerp(smoothedCameraPosition_, targetDrawCameraPosition, cameraFollowLerpRate_);
            smoothedLookAheadPosition_ = Lerp(smoothedLookAheadPosition_, targetLookAheadPositionDraw, cameraFollowLerpRate_);
        } else {
            smoothedCameraPosition_ = targetDrawCameraPosition;
            smoothedLookAheadPosition_ = targetLookAheadPositionDraw;
            hasCameraFollowState_ = true;
        }

        // cameraを行列再計算のためにLookAt設定
        camera_->LookAt(smoothedCameraPosition_, smoothedLookAheadPosition_);
    } else {
        hasCameraFollowState_ = false;
    }

    // 描画用カメラの行列を最新に確定
    camera_->Update();

    // 2. エイム用仮想カメラ（aimCamera_）の更新 (遅延なしの最新情報でLookAt)
    Vector3 targetCameraPosition = currentPosition - forward * kCameraBackwardOffset;
    targetCameraPosition.y += kCameraUpwardOffset;
    Vector3 targetLookAheadPosition = rail_->GetPositionByDistance(nextRailDistance + cameraLookAheadDistance_);
    
    aimCamera_->LookAt(targetCameraPosition, targetLookAheadPosition);
    aimCamera_->SetFovY(currentFovY_);
    aimCamera_->SetAspectRatio(camera_->GetAspectRatio());
    aimCamera_->SetNearClip(camera_->GetNearClip());
    aimCamera_->SetFarClip(camera_->GetFarClip());
    aimCamera_->Update();
}

void GamePlayScene::InitializeWaterPillars()
{
    auto addPillar = [this](float triggerDistance, float sideOffset, float delay) {
        const float pillarDistance = triggerDistance + 200.0f + delay * railSpeed_ * 60.0f;
        const Vector3 railPosition = rail_->GetPositionByDistance(pillarDistance);
        const Vector3 forward = CalculateRailForward(pillarDistance, railPosition);
        Vector3 right {};
        Vector3 up {};
        CalculateRailBasis(forward, right, up);
        Vector3 position = railPosition + right * sideOffset;
        position.y = stageSettings_.floorHeight;

        auto pillar = std::make_unique<WaterPillarHazard>();
        pillar->Initialize(waterPillarRenderer_.get(), position, triggerDistance, delay);
        waterPillars_.push_back(std::move(pillar));
    };

    addPillar(520.0f, 0.0f, 0.0f);
    addPillar(820.0f, -10.0f, 0.0f);
    addPillar(820.0f, 10.0f, 0.55f);
    addPillar(1130.0f, -13.0f, 0.0f);
    addPillar(1130.0f, 0.0f, 0.45f);
    addPillar(1130.0f, 13.0f, 0.90f);
    addPillar(1480.0f, 9.0f, 0.0f);
    addPillar(1480.0f, -9.0f, 0.65f);
}

void GamePlayScene::InitializeOceanLife()
{
    Model* fishModel = ModelManager::GetInstance()->Load("fish/fish.obj");
    Model* birdModel = ModelManager::GetInstance()->CreateBeamCross("resources/Textures/white.png");

    constexpr size_t kFishCount = 18;
    oceanFish_.reserve(kFishCount);
    for (size_t index = 0; index < kFishCount; ++index) {
        auto fish = std::make_unique<Object3d>();
        fish->Initialize(Object3dManager::GetInstance());
        fish->SetModel(fishModel);
        fish->SetScale({ 0.22f, 0.22f, 0.22f });
        fish->SetEnableLighting(true);
        oceanFish_.push_back(std::move(fish));
    }

    constexpr size_t kBirdCount = 10;
    oceanBirds_.reserve(kBirdCount);
    for (size_t index = 0; index < kBirdCount; ++index) {
        auto bird = std::make_unique<Object3d>();
        bird->Initialize(Object3dManager::GetInstance());
        bird->SetModel(birdModel);
        bird->SetScale({ 1.8f, 0.12f, 0.45f });
        bird->SetColor({ 0.92f, 0.96f, 1.0f, 1.0f });
        bird->SetEnableLighting(false);
        oceanBirds_.push_back(std::move(bird));
    }
}

void GamePlayScene::UpdateOceanLife(
    const Vector3& railPosition,
    const Vector3& forward,
    const Vector3& railRight)
{
    if (!oceanSurface_ || !player_) {
        return;
    }

    oceanLifeTime_ += TimeManager::GetInstance()->GetDeltaTime();
    const float seaHeight = stageSettings_.floorHeight;
    const float yaw = -std::atan2(forward.x, forward.z);

    constexpr float kFishSchoolDuration = 5.5f;
    if (!isFishSchoolActive_) {
        fishSchoolCooldown_ -= TimeManager::GetInstance()->GetDeltaTime();
        if (fishSchoolCooldown_ <= 0.0f) {
            isFishSchoolActive_ = true;
            fishSchoolTimer_ = 0.0f;
        }
    } else {
        fishSchoolTimer_ += TimeManager::GetInstance()->GetDeltaTime();
        const float progress = std::clamp(fishSchoolTimer_ / kFishSchoolDuration, 0.0f, 1.0f);
        const float travel = fishSchoolFromLeft_ ? (-52.0f + progress * 104.0f) : (52.0f - progress * 104.0f);
        const float crossYaw = yaw + (fishSchoolFromLeft_ ? -std::numbers::pi_v<float> * 0.5f : std::numbers::pi_v<float> * 0.5f);

        for (size_t index = 0; index < oceanFish_.size(); ++index) {
            const float phase = fishSchoolTimer_ * 3.2f + static_cast<float>(index) * 1.37f;
            const float formationSide = (static_cast<float>(index % 6) - 2.5f) * 1.7f;
            const float ahead = 34.0f + static_cast<float>(index % 6) * 7.0f +
                static_cast<float>(index / 6) * 4.0f;
            Vector3 position = railPosition + forward * ahead +
                railRight * (travel + formationSide);
            position.y = seaHeight + 0.45f + (std::max)(0.0f, std::sin(phase)) * 2.4f +
                static_cast<float>(index % 3) * 0.18f;
            oceanFish_[index]->SetTranslate(position);
            oceanFish_[index]->SetRotate({ -std::sin(phase) * 0.32f, crossYaw, 0.0f });
        }

        if (fishSchoolTimer_ >= kFishSchoolDuration) {
            isFishSchoolActive_ = false;
            fishSchoolFromLeft_ = !fishSchoolFromLeft_;
            fishSchoolCooldown_ = 12.0f + std::fmod(oceanLifeTime_ * 1.73f, 10.0f);
        }
    }

    for (size_t index = 0; index < oceanBirds_.size(); ++index) {
        const float phase = oceanLifeTime_ * (0.32f + static_cast<float>(index % 3) * 0.035f) +
            static_cast<float>(index) * 2.17f;
        const float side = (static_cast<float>(index % 5) - 2.0f) * 24.0f + std::sin(phase) * 12.0f;
        const float ahead = 95.0f + static_cast<float>(index % 5) * 34.0f;
        Vector3 position = railPosition + forward * ahead + railRight * side;
        position.y = seaHeight + 30.0f + static_cast<float>(index % 4) * 6.0f + std::sin(phase * 1.7f) * 2.0f;
        oceanBirds_[index]->SetTranslate(position);
        oceanBirds_[index]->SetRotate({ 0.0f, yaw, std::sin(phase * 3.2f) * 0.18f });
    }

}

void GamePlayScene::ProcessPlayerShooting(Input* input)
{
    if (input != nullptr && !debugCameraController_->GetDebugMode()) {
        if (input->IsMouseTrigger(0) && !player_->IsHomingMissileSelected()) {
            // 最新の描画用カメラを渡して、高精度な射撃用Rayから弾を発射する
            player_->FireBullet(*camera_);
        }
    }
}

void GamePlayScene::Draw3D()
{
    // skyBOx
    SkyBoxManager::GetInstance()->PreDraw();
    skyBox_->Draw(DirectXCommon::GetInstance()->GetCommandList());

    // OceanSurface owns a dedicated root signature and PSO, so draw it
    // before restoring the regular Object3d pipeline for gameplay objects.
    if (oceanSurface_) {
        oceanSurface_->Draw();
    }

    Object3dManager::GetInstance()->PreDraw();

    // Object3dManager::GetInstance()->SetGlowPSO();
    // Object3dManager::GetInstance()->SetNormalPSO();
    // Object3dManager::GetInstance()->SetBlendMode(kBlendModeMultiply);
    // terrain_->Draw();
    for (std::unique_ptr<Object3d>& levelObject : levelObjects_) {
        levelObject->Draw();
    }
    for (RecoveryItem& recoveryItem : recoveryItems_) {
        if (!recoveryItem.collected && recoveryItem.object != nullptr) {
            recoveryItem.object->Draw();
        }
    }
    player_->Draw();
    sceneObjectManager_->Draw();
    if (floorObj_) {
        floorObj_->Draw();
    }
    if (isFishSchoolActive_) {
        for (std::unique_ptr<Object3d>& fish : oceanFish_) fish->Draw();
    }
    for (std::unique_ptr<Object3d>& bird : oceanBirds_) bird->Draw();
    if (waterPillarRenderer_) {
        waterPillarRenderer_->PreDraw();
        for (std::unique_ptr<WaterPillarHazard>& pillar : waterPillars_) pillar->DrawPillar();
        Object3dManager::GetInstance()->PreDraw();
    }
    for (std::unique_ptr<BaseEnemy>& enemy : enemies_) {
        enemy->Draw();
    }
    enemyBulletManager_.Draw();

    if (GetActiveBoss() != nullptr) {
        GetActiveBoss()->Draw();
    }
#if defined(_DEBUG) || defined(ENABLE_DEVELOPMENT_TOOLS)
    if (DebugRenderer::GetInstance()->IsVisible()) {
        if (showRailDebug_) {
            rail_->DrawDebug();
        }
        if (showCollisionDebug_) {
            DrawCollisionDebug();
        }
    }
#endif

    //----------------------
    // スキニング
    //----------------------
    SkinningObject3dManager::GetInstance()->PreDraw();
                                                                                        // animationSkin00_->Draw();
    animationActor_->Draw();
}

void GamePlayScene::DrawParticle()
{
    EffectManager::GetInstance()->PreDraw();
    EffectManager::GetInstance()->Draw();
}

void GamePlayScene::InitializeRecoveryItems(Model* model)
{
    if (model == nullptr) {
        return;
    }

    recoveryItems_.clear();
    recoveryItems_.reserve(stageSettings_.recoveryItemPositions.size());

    for (const Vector3& position : stageSettings_.recoveryItemPositions) {
        RecoveryItem recoveryItem {};
        recoveryItem.object = std::make_unique<Object3d>();
        recoveryItem.object->Initialize(Object3dManager::GetInstance());
        recoveryItem.object->SetModel(model);
        recoveryItem.object->SetTranslate(position);
        recoveryItem.object->SetScale({ 0.75f, 0.75f, 0.75f });
        recoveryItem.object->SetColor(kRecoveryMint);
        recoveryItem.object->SetEnableLighting(false);
        recoveryItem.basePosition = position;
        recoveryItem.object->Update();
        recoveryItem.effectHandle =
            EffectManager::GetInstance()->PlayLoopEffect(
                "HealPickup",
                position);
        recoveryItems_.push_back(std::move(recoveryItem));
    }
}

void GamePlayScene::UpdateRecoveryItems()
{
    if (player_ == nullptr) {
        return;
    }

    const Vector3 playerPosition = player_->GetTranslate();
    const float collisionRadiusSquared =
        kRecoveryItemCollisionRadius * kRecoveryItemCollisionRadius;

    for (RecoveryItem& recoveryItem : recoveryItems_) {
        if (recoveryItem.collected || recoveryItem.object == nullptr) {
            continue;
        }

        recoveryItem.animationTime += kRecoveryItemBobSpeed;

        Vector3 itemPosition = recoveryItem.basePosition;
        itemPosition.y +=
            std::sin(recoveryItem.animationTime) * kRecoveryItemBobHeight;

        Vector3 itemRotation = recoveryItem.object->GetRotate();
        itemRotation.x += kRecoveryItemRotationSpeed * 0.65f;
        itemRotation.y += kRecoveryItemRotationSpeed;
        recoveryItem.object->SetTranslate(itemPosition);
        recoveryItem.object->SetRotate(itemRotation);
        recoveryItem.object->Update();
        if (recoveryItem.effectHandle != kInvalidEffectHandle) {
            EffectManager::GetInstance()->SetEffectPosition(
                recoveryItem.effectHandle,
                itemPosition);
        }

        const float differenceX = playerPosition.x - itemPosition.x;
        const float differenceY = playerPosition.y - itemPosition.y;
        const float differenceZ = playerPosition.z - itemPosition.z;
        const float distanceSquared =
            differenceX * differenceX +
            differenceY * differenceY +
            differenceZ * differenceZ;

        if (distanceSquared > collisionRadiusSquared) {
            continue;
        }

        if (!player_->Heal(kRecoveryItemHealAmount)) {
            continue;
        }

        recoveryItem.collected = true;
        EffectManager::GetInstance()->StopEffect(
            recoveryItem.effectHandle);
        recoveryItem.effectHandle = kInvalidEffectHandle;
        EffectManager::GetInstance()->PlayEffect(
            "HealPickup",
            itemPosition);
    }
}

void GamePlayScene::Draw2D()
{
    SpriteManager::GetInstance()->PreDraw();
    // testSprite_->Draw();
    if (GetActiveBoss() == nullptr || !GetActiveBoss()->IsDead()) {
        aimSprite_->Draw();
        std::vector<Vector3> homingLockPositions;
        player_->GetHomingLockPositions(homingLockPositions);
        const size_t markerCount =
            (std::min)(homingLockPositions.size(), homingLockSprites_.size());
        for (size_t index = 0; index < markerCount; ++index) {
            homingLockSprites_[index]->Draw();
        }
    }

    // 全HUDは同じ濃紺パネル・アイボリー罫線・機能色アクセントを使う。
    DrawHudFrame(weaponHudFrameSprites_);
    if (weaponHudBgSprite_) weaponHudBgSprite_->Draw();
    DrawHudFrame(playerHudFrameSprites_);
    if (playerHpBgSprite_) playerHpBgSprite_->Draw();
    if (playerHpBarSprite_) playerHpBarSprite_->Draw();

    if (GetActiveBoss() != nullptr && !GetActiveBoss()->IsDeathSequenceFinished()) {
        DrawHudFrame(bossHudFrameSprites_);
        if (bossHeadHpBgSprite_) bossHeadHpBgSprite_->Draw();
        if (bossHeadHpBarSprite_) bossHeadHpBarSprite_->Draw();
        if (bossBodyHpBgSprite_) bossBodyHpBgSprite_->Draw();
        if (bossBodyHpBarSprite_) bossBodyHpBarSprite_->Draw();
    }

    if (isPaused_) {
        if (pauseMenuPanelSprite_) pauseMenuPanelSprite_->Draw();
        if (pauseResumeBtnSprite_) pauseResumeBtnSprite_->Draw();
        if (pauseRetryBtnSprite_) pauseRetryBtnSprite_->Draw();
        if (pauseTitleBtnSprite_) pauseTitleBtnSprite_->Draw();
        if (pauseControlBtnSprite_) pauseControlBtnSprite_->Draw();

        // 独自TextRendererによるRelease構成対応の超高画質日本語テキスト描画
        TextRenderer::GetInstance()->PreDraw();
        if (pauseTitleText_) pauseTitleText_->Draw();
        if (pauseResumeText_) pauseResumeText_->Draw();
        if (pauseRetryText_) pauseRetryText_->Draw();
        if (pauseTitleBtnText_) pauseTitleBtnText_->Draw();
        if (pauseControlText_) pauseControlText_->Draw();
        if (pauseSensitivityText_) pauseSensitivityText_->Draw();
    } else {
        // 通常プレイ中の画面右上HP数値テキストの描画
        TextRenderer::GetInstance()->PreDraw();
        if (weaponHudLabelText_) weaponHudLabelText_->Draw();
        if (weaponHudNameText_) weaponHudNameText_->Draw();
        if (playerHpText_) playerHpText_->Draw();
        if (GetActiveBoss() != nullptr && !GetActiveBoss()->IsDeathSequenceFinished()) {
            if (bossNameText_) bossNameText_->Draw();
            if (bossHeadHpText_) bossHeadHpText_->Draw();
            if (bossBodyHpText_) bossBodyHpText_->Draw();
        }
    }
}

void GamePlayScene::UpdateBossHpHud()
{
    if (GetActiveBoss() == nullptr) {
        return;
    }

    float headHpFraction = GetActiveBoss()->GetHeadHpFraction();
    if (headHpFraction < 0.0f) {
        headHpFraction = 0.0f;
    }
    if (headHpFraction > 1.0f) {
        headHpFraction = 1.0f;
    }

    float bodyHpFraction = GetActiveBoss()->GetBodyHpFraction();
    if (bodyHpFraction < 0.0f) {
        bodyHpFraction = 0.0f;
    }
    if (bodyHpFraction > 1.0f) {
        bodyHpFraction = 1.0f;
    }

    displayedBossHeadHpRatio_ = std::lerp(
        displayedBossHeadHpRatio_,
        headHpFraction,
        0.10f);
    displayedBossBodyHpRatio_ = std::lerp(
        displayedBossBodyHpRatio_,
        bodyHpFraction,
        0.10f);

    if (std::abs(displayedBossHeadHpRatio_ - headHpFraction) < 0.001f) {
        displayedBossHeadHpRatio_ = headHpFraction;
    }
    if (std::abs(displayedBossBodyHpRatio_ - bodyHpFraction) < 0.001f) {
        displayedBossBodyHpRatio_ = bodyHpFraction;
    }

    constexpr float kBossHpBarWidth = 340.0f;
    if (bossHeadHpBarSprite_) {
        bossHeadHpBarSprite_->SetSize({
            kBossHpBarWidth * displayedBossHeadHpRatio_,
            14.0f });
        bossHeadHpBarSprite_->Update();
    }
    if (bossBodyHpBarSprite_) {
        bossBodyHpBarSprite_->SetSize({
            kBossHpBarWidth * displayedBossBodyHpRatio_,
            14.0f });
        bossBodyHpBarSprite_->Update();
    }
    if (bossHeadHpBgSprite_) {
        bossHeadHpBgSprite_->Update();
    }
    if (bossBodyHpBgSprite_) {
        bossBodyHpBgSprite_->Update();
    }
    if (bossNameText_) {
        bossNameText_->Update();
    }
    if (bossHeadHpText_) {
        bossHeadHpText_->Update();
    }
    if (bossBodyHpText_) {
        bossBodyHpText_->Update();
    }
}

void GamePlayScene::DrawImGui()
{
#ifdef USE_IMGUI
    if (isPaused_) {
        ImGui::SetNextWindowPos(ImVec2(WinApp::kClientWidth * 0.5f, WinApp::kClientHeight * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(400.0f, 520.0f));

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | 
                                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar |
                                 ImGuiWindowFlags_NoBackground;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 1.0f, 1.0f, 0.25f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0f, 1.0f, 1.0f, 0.40f));

        if (ImGui::Begin("VerticalPauseWindow", nullptr, flags)) {
            ImGui::SetWindowFontScale(1.4f);
            ImGui::Spacing();
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize("PAUSE MENU").x * 1.4f) * 0.5f);
            ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.0f, 1.0f), "PAUSE MENU");
            ImGui::Separator();

            ImGui::SetWindowFontScale(1.25f);

            // 1. RESUME (TAB)
            ImGui::SetCursorPosY(110.0f);
            if (ImGui::Button("RESUME (TAB)", ImVec2(-1, 44.0f))) {
                isPaused_ = false;
            }

            // 2. RETRY (R)
            ImGui::SetCursorPosY(180.0f);
            if (ImGui::Button("RETRY (R)", ImVec2(-1, 44.0f))) {
                isPaused_ = false;
                ResetGameplayPostEffects();
                SceneManager::GetInstance()->SetNextScene(
                    std::make_unique<GamePlayScene>(stageId_));
            }

            // 3. CONTROL MODE
            ImGui::SetCursorPosY(250.0f);
            const char* controlLabel =
                gControlMode == Player::ControlMode::StarFox
                    ? "CONTROL: STARFOX (C)"
                    : "CONTROL: WASD + MOUSE (C)";
            if (ImGui::Button(controlLabel, ImVec2(-1, 44.0f))) {
                gControlMode = gControlMode == Player::ControlMode::KeyboardAndMouse
                    ? Player::ControlMode::StarFox
                    : Player::ControlMode::KeyboardAndMouse;
                if (player_) {
                    player_->SetControlMode(gControlMode);
                }
            }

            ImGui::SetCursorPosY(315.0f);
            if (ImGui::SliderFloat(
                    "MOUSE SENSITIVITY",
                    &gMouseSensitivity,
                    0.5f,
                    2.0f,
                    "%.1fx")) {
                if (player_) {
                    player_->SetMouseSensitivity(gMouseSensitivity);
                }
            }

            // 5. TITLE (ESC)
            ImGui::SetCursorPosY(390.0f);
            if (ImGui::Button("TITLE (ESC)", ImVec2(-1, 44.0f))) {
                isPaused_ = false;
                ResetGameplayPostEffects();
                SceneManager::GetInstance()->SetNextScene(std::make_unique<TitleScene>());
            }

            ImGui::End();
        }

        ImGui::PopStyleColor(6);
        ImGui::PopStyleVar(1);
        return;
    }
#endif
#if defined(ENABLE_DEVELOPMENT_TOOLS) && !defined(_DEBUG)
    // Keep the TAB pause menu. F10 can reveal older ImGui tools when needed.
    if (!DevelopmentWebPanel::GetInstance().IsLegacyUiVisible()) {
        editorManager_->DrawGizmo(camera_.get());
        return;
    }
#endif
#ifdef USE_IMGUI
    // ボス出現時、画面上部中央にスタイリッシュな2本の横長HPバーをHUD風にオーバーレイ表示する
    if (GetActiveBoss() != nullptr && !GetActiveBoss()->IsDeathSequenceFinished()) {
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        // 画面上部中央付近に横幅550pxで表示
        ImVec2 windowPos = ImVec2(viewport->Pos.x + viewport->Size.x * 0.5f - 275.0f, viewport->Pos.y + 40.0f);
        ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(550.0f, 95.0f), ImGuiCond_Always);
        
        // 背景・タイトルバー・枠線などを非表示にして、HUDスプライトのように見せる
        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoTitleBar | 
                                       ImGuiWindowFlags_NoResize | 
                                       ImGuiWindowFlags_NoMove | 
                                       ImGuiWindowFlags_NoScrollbar | 
                                       ImGuiWindowFlags_NoSavedSettings | 
                                       ImGuiWindowFlags_NoBackground;

        if (ImGui::Begin("Boss HP HUD", nullptr, windowFlags)) {
            // ボス名称
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
            const char* debugBossName = stageSettings_.bossType == "AngerBlock"
                ? "ANGER"
                : (stageSettings_.bossType == "IceJellyfish" ? "ICE JELLYFISH" : "FEAR WORM");
            ImGui::Text("BOSS: %s", debugBossName);
            ImGui::PopStyleColor();

            // 1. 頭部HPバー (ネオンブルー)
            float headFraction = GetActiveBoss()->GetHeadHpFraction();
            ImGui::Text(stageSettings_.bossType == "IceJellyfish" ? "CORE HP  " : "HEAD CORE  ");
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.2f, 0.6f, 1.0f, 1.0f)); // ネオンブルー
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.1f, 0.2f, 0.3f, 0.4f));       // 暗い青背景
            ImGui::ProgressBar(headFraction, ImVec2(-1, 14.0f), "");
            ImGui::PopStyleColor(2);

            // 2. 胴体HPバー (ネオンレッド + 胴体数に応じた9分割の区切り線)
            float bodyFraction = GetActiveBoss()->GetBodyHpFraction();
            ImGui::Text(stageSettings_.bossType == "IceJellyfish" ? "TENTACLES  " : "BODY SHIELD");
            ImGui::SameLine();
            
            ImVec2 barPosMin = ImGui::GetCursorScreenPos();
            
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(1.0f, 0.2f, 0.2f, 1.0f)); // ネオンレッド
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.3f, 0.1f, 0.1f, 0.4f));       // 暗い赤背景
            ImGui::ProgressBar(bodyFraction, ImVec2(-1, 14.0f), "");
            ImGui::PopStyleColor(2);

            // 直前に描画したProgressBarの領域を取得して、9分割(8本の縦線)で区切る
            ImVec2 barPosMax = ImGui::GetItemRectMax();
            float barWidth = barPosMax.x - barPosMin.x;
            const int kSegmentDivisions = stageSettings_.bossType == "IceJellyfish" ? 6 : 9;
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            ImU32 lineColor = IM_COL32(10, 10, 10, 255); // ほぼ黒のシャープな区切り線

            for (int i = 1; i < kSegmentDivisions; ++i) {
                float splitX = barPosMin.x + (barWidth * i / static_cast<float>(kSegmentDivisions));
                drawList->AddLine(
                    ImVec2(splitX, barPosMin.y),
                    ImVec2(splitX, barPosMax.y),
                    lineColor,
                    2.0f // 2pxの太さでしっかり区切る
                );
            }
        }
        ImGui::End();
    }

    camera_->DrawImGui();
    editorManager_->DrawImGui();
    editorManager_->DrawGizmo(camera_.get());
    player_->DrawImGui();

    ImGui::Begin("MoveEnemy Adjuster");
    int32_t moveEnemyCount = 0;
    for (std::unique_ptr<BaseEnemy>& enemy : enemies_) {
        if (!enemy->IsDead()) {
            MoveEnemy* moveEnemy = dynamic_cast<MoveEnemy*>(enemy.get());
            if (moveEnemy != nullptr) {
                char label[64];
                sprintf_s(label, "MoveEnemy [%d]", moveEnemyCount);
                if (ImGui::TreeNode(label)) {
                    moveEnemy->DrawImGui();
                    ImGui::TreePop();
                }
                moveEnemyCount = moveEnemyCount + 1;
            }
        }
    }
    if (moveEnemyCount == 0) {
        ImGui::Text("No active MoveEnemy found.");
    }
    ImGui::End();

    ImGui::Begin("Camera Adjuster");
    ImGui::DragFloat("Height Follow Factor", &cameraHeightFollowFactor_, 0.01f, 0.0f, 1.0f);
    ImGui::DragFloat("Look Up Factor", &cameraLookUpFactor_, 0.01f, 0.0f, 2.0f);
    ImGui::End();

    ImGui::Begin("Debug Teleport Menu");
    if (IceJellyfish* iceJellyfish = dynamic_cast<IceJellyfish*>(GetActiveBoss())) {
        ImGui::Text("Ice Jellyfish core HP: %.0f / %.0f", iceJellyfish->GetHp(), IceJellyfish::kMaxHp);
        ImGui::Checkbox("Show Ice Jellyfish collision", &showIceJellyfishCollision_);
        if (ImGui::Button("Teleport to Ice Jellyfish")) {
            railDistance_ = (std::max)(0.0f, stageSettings_.bossPosition.z - 200.0f);
        }
    }
    if (GetActiveBoss() != nullptr) {
        ImGui::Text("Boss Z: %.2f", GetActiveBoss()->GetPosition().z);
        if (ImGui::Button("Teleport to Boss")) {
            float bossZ = GetActiveBoss()->GetPosition().z;
            railDistance_ = bossZ - 130.0f;
            if (railDistance_ < 0.0f) {
                railDistance_ = 0.0f;
            }
        }
    } else {
        ImGui::Text("Boss has not spawned yet.");
        if (ImGui::Button("Warp to Boss Area (Trigger Spawn)")) {
            railDistance_ = 1750.0f;
        }
    }
    ImGui::End();
#endif
}

void GamePlayScene::CheckCollision()
{
    bool justDodgedEnemyBullet = false;

    if (gameplayCollisionSystem_ != nullptr) {
        gameplayCollisionSystem_->UpdateStageCollisions(
            *player_,
            levelObjects_,
            destructibleLevelObjects_,
            floorObj_.get());
    }

    if (gameplayCollisionSystem_ != nullptr) {
        const GameplayCollisionEvents events =
            gameplayCollisionSystem_->UpdateCombatCollisions(
                *player_,
                enemies_,
                GetActiveBoss(),
                enemyBulletManager_.GetBullets());
        if (events.paintBulletHitPlayer) {
            StartPaintHitEffect();
        }
        justDodgedEnemyBullet = events.justDodgedEnemyBullet;
    }

    if (gameplayCollisionSystem_ != nullptr) {
        gameplayCollisionSystem_->UpdateTriggers(*player_, stageTriggers_);
    }

    UpdateJustDodgeSlowMotion(justDodgedEnemyBullet);
}

void GamePlayScene::UpdateJustDodgeSlowMotion(bool justDodged)
{
    if (justDodged) {
        justDodgeSlowTimer_ = kJustDodgeSlowDuration;
    } else if (justDodgeSlowTimer_ > 0.0f) {
        justDodgeSlowTimer_ -= TimeManager::GetInstance()->GetDeltaTime();
        if (justDodgeSlowTimer_ < 0.0f) {
            justDodgeSlowTimer_ = 0.0f;
        }
    }

    if (justDodgeSlowTimer_ > 0.0f) {
        EnemyBullet::SetTimeScale(kJustDodgeEnemyBulletTimeScale);
        SceneManager::GetInstance()->AddPostEffect(
            PostEffectType::GrayScale,
            PostEffectStage::BeforeParticle);
    } else {
        EnemyBullet::SetTimeScale(1.0f);
        if (!isPaused_) {
            SceneManager::GetInstance()->RemovePostEffect(
                PostEffectType::GrayScale);
        }
    }
}

void GamePlayScene::StartPaintHitEffect()
{
    if (isPaintEffectActive_) {
        return;
    }

    isPaintEffectActive_ = true;
    paintEffectTimer_ = 0.0f;
    static const Vector3 kPaintColors[] = {
        { 0.98f, 0.12f, 0.60f },
        { 0.10f, 0.88f, 0.95f },
        { 0.98f, 0.88f, 0.10f },
        { 0.20f, 0.95f, 0.35f },
        { 0.98f, 0.42f, 0.10f },
        { 0.72f, 0.15f, 0.98f }
    };
    const int colorIndex = rand() % 6;
    const float randomSeed =
        static_cast<float>(rand() % 10000) * 0.137f;

    int patternType = 0;
    const int roll = rand() % 10;
    if (roll < 3) {
        patternType = 1;
    } else if (roll < 5) {
        patternType = 2;
    } else if (roll < 7) {
        patternType = 3;
    }

    SceneManager::GetInstance()->SetPaintColor(kPaintColors[colorIndex]);
    SceneManager::GetInstance()->SetPaintSeed(randomSeed);
    SceneManager::GetInstance()->SetPaintPatternType(patternType);
    SceneManager::GetInstance()->AddPostEffect(
        PostEffectType::Paint,
        PostEffectStage::AfterParticle);
    SceneManager::GetInstance()->SetPaintProgress(0.0f);
    SceneManager::GetInstance()->SetPaintIntensity(1.0f);
}

void GamePlayScene::StartWaterDropEffect()
{
    waterDropEffectTimer_ = kWaterDropEffectDuration;
    SceneManager::GetInstance()->SetWaterEffectIntensity(1.0f);
}

void GamePlayScene::UpdateWaterDropEffect()
{
    SceneManager* sceneManager = SceneManager::GetInstance();
    if (waterDropEffectTimer_ <= 0.0f) {
        waterDropEffectTimer_ = 0.0f;
        sceneManager->SetWaterEffectIntensity(0.0f);
        sceneManager->RemovePostEffect(PostEffectType::RainDrops);
        return;
    }

    waterDropEffectTimer_ -= TimeManager::GetInstance()->GetDeltaTime();
    if (waterDropEffectTimer_ < 0.0f) {
        waterDropEffectTimer_ = 0.0f;
    }

    const float fadeDuration = 2.0f;
    float intensity = 1.0f;
    if (waterDropEffectTimer_ < fadeDuration) {
        intensity = waterDropEffectTimer_ / fadeDuration;
    }
    sceneManager->SetWaterEffectIntensity(intensity);

    sceneManager->AddPostEffect(
        PostEffectType::RainDrops,
        PostEffectStage::AfterParticle);
}

#if defined(ENABLE_DEVELOPMENT_TOOLS)
std::string GamePlayScene::GetDevelopmentStateJson() const
{
    const StageBoss* boss = GetActiveBoss();
    const IceJellyfish* jellyfish = dynamic_cast<const IceJellyfish*>(boss);
    nlohmann::json state = {
        { "active", true },
        { "freeCamera", debugCameraController_->GetDebugMode() },
        { "cameraSpeed", debugCameraController_->GetMoveSpeed() },
        { "paused", developmentPaused_ },
        { "invincibleMode", player_->IsInvincibleMode() },
        { "playerHp", player_->GetCurrentHp() },
        { "playerMaxHp", player_->GetMaxHp() },
        { "showRail", showRailDebug_ },
        { "showCollision", showCollisionDebug_ },
        { "showPlayer", showPlayerCollision_ },
        { "showEnemy", showEnemyCollision_ },
        { "showStage", showStageCollision_ },
        { "showBullet", showBulletCollision_ },
        { "showIce", showIceJellyfishCollision_ },
        { "controlMode", static_cast<int>(gControlMode) },
        { "mouseSensitivity", gMouseSensitivity },
        { "cameraHeightFollowFactor", cameraHeightFollowFactor_ },
        { "cameraLookUpFactor", cameraLookUpFactor_ },
        { "cameraX", camera_->GetTranslate().x }, { "cameraY", camera_->GetTranslate().y },
        { "cameraZ", camera_->GetTranslate().z },
        { "cameraRotX", camera_->GetRotate().x }, { "cameraRotY", camera_->GetRotate().y },
        { "cameraRotZ", camera_->GetRotate().z },
        { "cameraScaleX", camera_->GetScale().x }, { "cameraScaleY", camera_->GetScale().y },
        { "cameraScaleZ", camera_->GetScale().z },
        { "cameraFovY", camera_->GetFovY() }, { "cameraNearClip", camera_->GetNearClip() },
        { "cameraFarClip", camera_->GetFarClip() },
        { "lightEnabled", lightEnabled_ },
        { "lightColorR", lightColor_.x }, { "lightColorG", lightColor_.y }, { "lightColorB", lightColor_.z },
        { "lightIntensity", lightIntensity_ },
        { "lightDirX", lightDir_.x }, { "lightDirY", lightDir_.y }, { "lightDirZ", lightDir_.z },
        { "ambientColorR", ambientColor_.x }, { "ambientColorG", ambientColor_.y },
        { "ambientColorB", ambientColor_.z }, { "ambientIntensity", ambientColor_.w },
        { "pointEnabled", pointEnabled_ },
        { "pointColorR", pointColor_.x }, { "pointColorG", pointColor_.y }, { "pointColorB", pointColor_.z },
        { "pointPosX", pointPos_.x }, { "pointPosY", pointPos_.y }, { "pointPosZ", pointPos_.z },
        { "pointIntensity", pointIntensity_ }, { "pointRadius", pointRadius_ }, { "pointDecay", pointDecay_ },
        { "spotEnabled", spotEnabled_ },
        { "spotColorR", spotColor_.x }, { "spotColorG", spotColor_.y }, { "spotColorB", spotColor_.z },
        { "spotPosX", spotPos_.x }, { "spotPosY", spotPos_.y }, { "spotPosZ", spotPos_.z },
        { "spotDirX", spotDir_.x }, { "spotDirY", spotDir_.y }, { "spotDirZ", spotDir_.z },
        { "spotIntensity", spotIntensity_ }, { "spotDistance", spotDistance_ },
        { "spotDecay", spotDecay_ }, { "spotAngleDeg", spotAngleDeg_ },
        { "spotFalloffStartDeg", spotFalloffStartDeg_ },
        { "collisionOverlay", collisionOverlay_ },
        { "drawDistance", collisionDrawDistance_ },
        { "railDistance", railDistance_ },
        { "bossAvailable", boss != nullptr && !boss->IsDead() },
        { "jellyfishAvailable", jellyfish != nullptr && !jellyfish->IsDead() },
        { "bossZ", boss ? boss->GetPosition().z : 0.0f },
        { "bossHp", jellyfish ? nlohmann::json(jellyfish->GetHp()) : nlohmann::json(nullptr) }
    };
    state["moveEnemies"] = nlohmann::json::array();
    state["objects"] = nlohmann::json::array();
    state["selectedObject"] = nullptr;
    state["gizmoMode"] = static_cast<int>(editorManager_->GetGizmoMode());
    const auto& sceneObjects = sceneObjectManager_->GetObjects();
    for (std::size_t index = 0; index < sceneObjects.size(); ++index) {
        const Object3d* object = sceneObjects[index].get();
        state["objects"].push_back({{"index", index}, {"name", object->GetName()}});
        if (object == editorManager_->GetSelectedObject()) {
            state["selectedObject"] = index;
            state["objectX"] = object->GetTranslate().x;
            state["objectY"] = object->GetTranslate().y;
            state["objectZ"] = object->GetTranslate().z;
            state["objectRotX"] = object->GetRotate().x;
            state["objectRotY"] = object->GetRotate().y;
            state["objectRotZ"] = object->GetRotate().z;
            state["objectScaleX"] = object->GetScale().x;
            state["objectScaleY"] = object->GetScale().y;
            state["objectScaleZ"] = object->GetScale().z;
        }
    }
    int moveEnemyIndex = 0;
    for (const auto& enemy : enemies_) {
        if (enemy->IsDead()) continue;
        if (const MoveEnemy* moveEnemy = dynamic_cast<const MoveEnemy*>(enemy.get())) {
            state["moveEnemies"].push_back({
                {"index", moveEnemyIndex++},
                {"pattern", static_cast<int>(moveEnemy->GetMovePattern())},
                {"speed", moveEnemy->GetMoveSpeed()},
                {"amplitude", moveEnemy->GetAmplitude()},
                {"frequency", moveEnemy->GetFrequency()}
            });
        }
    }
    return state.dump();
}

void GamePlayScene::ApplyDevelopmentAction(const std::string& key, const std::string& value)
{
    const bool enabled = value == "true";
    if (key == "invincibleMode") {
        player_->SetInvincibleMode(enabled);
        return;
    }
    if (key == "editorSave") { editorManager_->SaveJson("resources/Scenes/TestScene.json"); return; }
    if (key == "editorLoad") { editorManager_->LoadJson("resources/Scenes/TestScene.json"); return; }
    if (key == "objectSelect" || key == "gizmoMode") {
        char* end = nullptr;
        const long index = std::strtol(value.c_str(), &end, 10);
        if (end == value.c_str() || *end != '\0') return;
        if (key == "gizmoMode") {
            if (index >= 0 && index <= 2) editorManager_->SetGizmoMode(static_cast<GizmoMode>(index));
        } else if (index == -1) {
            editorManager_->SetSelectedObject(nullptr);
        } else if (index >= 0 && static_cast<std::size_t>(index) < sceneObjectManager_->GetObjects().size()) {
            editorManager_->SetSelectedObject(sceneObjectManager_->GetObjects()[index].get());
        }
        return;
    }
    if (key.starts_with("object")) {
        Object3d* object = editorManager_->GetSelectedObject();
        if (!object) return;
        char* end = nullptr;
        const float number = std::strtof(value.c_str(), &end);
        if (end == value.c_str() || *end != '\0' || !std::isfinite(number)) return;
        Vector3 translate = object->GetTranslate();
        Vector3 rotate = object->GetRotate();
        Vector3 scale = object->GetScale();
#define OBJECT_FIELD(name, field, low, high) if (key == name) { field = std::clamp(number, low, high); }
        OBJECT_FIELD("objectX", translate.x, -10000.0f, 10000.0f)
        OBJECT_FIELD("objectY", translate.y, -10000.0f, 10000.0f)
        OBJECT_FIELD("objectZ", translate.z, -10000.0f, 10000.0f)
        OBJECT_FIELD("objectRotX", rotate.x, -6.28f, 6.28f)
        OBJECT_FIELD("objectRotY", rotate.y, -6.28f, 6.28f)
        OBJECT_FIELD("objectRotZ", rotate.z, -6.28f, 6.28f)
        OBJECT_FIELD("objectScaleX", scale.x, 0.01f, 100.0f)
        OBJECT_FIELD("objectScaleY", scale.y, 0.01f, 100.0f)
        OBJECT_FIELD("objectScaleZ", scale.z, 0.01f, 100.0f)
#undef OBJECT_FIELD
        object->SetTranslate(translate);
        object->SetRotate(rotate);
        object->SetScale(scale);
        return;
    }
    if (key.starts_with("enemy.")) {
        const std::size_t separator = key.find('.', 6);
        if (separator == std::string::npos) return;
        const std::string indexText = key.substr(6, separator - 6);
        char* indexEnd = nullptr;
        const long targetIndex = std::strtol(indexText.c_str(), &indexEnd, 10);
        if (indexEnd == indexText.c_str() || *indexEnd != '\0' || targetIndex < 0) return;
        int currentIndex = 0;
        for (const auto& enemy : enemies_) {
            if (enemy->IsDead()) continue;
            MoveEnemy* moveEnemy = dynamic_cast<MoveEnemy*>(enemy.get());
            if (!moveEnemy) continue;
            if (currentIndex++ != targetIndex) continue;
            const std::string field = key.substr(separator + 1);
            if (field == "reset") { moveEnemy->ResetPosition(); return; }
            char* end = nullptr;
            const float number = std::strtof(value.c_str(), &end);
            if (end == value.c_str() || *end != '\0' || !std::isfinite(number)) return;
            if (field == "pattern") moveEnemy->SetMovePattern(static_cast<MovePattern>(std::clamp(static_cast<int>(number), 0, 3)));
            else if (field == "speed") moveEnemy->SetMoveSpeed(std::clamp(number, 0.0f, 10.0f));
            else if (field == "amplitude") moveEnemy->SetAmplitude(std::clamp(number, 0.0f, 20.0f));
            else if (field == "frequency") moveEnemy->SetFrequency(std::clamp(number, 0.0f, 10.0f));
            return;
        }
        return;
    }
    if (key == "lightEnabled") { lightEnabled_ = enabled; ApplyDevelopmentLighting(); return; }
    if (key == "pointEnabled") { pointEnabled_ = enabled; ApplyDevelopmentLighting(); return; }
    if (key == "spotEnabled") { spotEnabled_ = enabled; ApplyDevelopmentLighting(); return; }
    if (key == "resetLightDirection") {
        lightDir_ = { -0.35f, -0.82f, 0.45f };
        ApplyDevelopmentLighting();
        return;
    }
    if (key == "resetLight") {
        lightEnabled_ = true;
        lightColor_ = { 0.90f, 0.96f, 1.0f, 1.0f };
        lightIntensity_ = 0.90f;
        lightDir_ = { -0.35f, -0.82f, 0.45f };
        ambientColor_ = { 0.40f, 0.52f, 0.68f, 0.24f };
        ApplyDevelopmentLighting();
        return;
    }
    char* settingEnd = nullptr;
    const float number = std::strtof(value.c_str(), &settingEnd);
    if (settingEnd != value.c_str() && *settingEnd == '\0' && std::isfinite(number)) {
#define SET_LIGHT(name, field, low, high) if (key == name) { field = std::clamp(number, low, high); ApplyDevelopmentLighting(); return; }
#define SET_VALUE(name, field, low, high) if (key == name) { field = std::clamp(number, low, high); return; }
        SET_LIGHT("lightColorR", lightColor_.x, 0.0f, 1.0f)
        SET_LIGHT("lightColorG", lightColor_.y, 0.0f, 1.0f)
        SET_LIGHT("lightColorB", lightColor_.z, 0.0f, 1.0f)
        SET_LIGHT("lightIntensity", lightIntensity_, 0.0f, 5.0f)
        SET_LIGHT("lightDirX", lightDir_.x, -1.0f, 1.0f)
        SET_LIGHT("lightDirY", lightDir_.y, -1.0f, 1.0f)
        SET_LIGHT("lightDirZ", lightDir_.z, -1.0f, 1.0f)
        SET_LIGHT("ambientColorR", ambientColor_.x, 0.0f, 1.0f)
        SET_LIGHT("ambientColorG", ambientColor_.y, 0.0f, 1.0f)
        SET_LIGHT("ambientColorB", ambientColor_.z, 0.0f, 1.0f)
        SET_LIGHT("ambientIntensity", ambientColor_.w, 0.0f, 1.0f)
        SET_LIGHT("pointColorR", pointColor_.x, 0.0f, 1.0f)
        SET_LIGHT("pointColorG", pointColor_.y, 0.0f, 1.0f)
        SET_LIGHT("pointColorB", pointColor_.z, 0.0f, 1.0f)
        SET_LIGHT("pointPosX", pointPos_.x, -10.0f, 10.0f)
        SET_LIGHT("pointPosY", pointPos_.y, -10.0f, 10.0f)
        SET_LIGHT("pointPosZ", pointPos_.z, -10.0f, 10.0f)
        SET_LIGHT("pointIntensity", pointIntensity_, 0.0f, 5.0f)
        SET_LIGHT("pointRadius", pointRadius_, 0.1f, 30.0f)
        SET_LIGHT("pointDecay", pointDecay_, 0.1f, 5.0f)
        SET_LIGHT("spotColorR", spotColor_.x, 0.0f, 1.0f)
        SET_LIGHT("spotColorG", spotColor_.y, 0.0f, 1.0f)
        SET_LIGHT("spotColorB", spotColor_.z, 0.0f, 1.0f)
        SET_LIGHT("spotPosX", spotPos_.x, -10.0f, 10.0f)
        SET_LIGHT("spotPosY", spotPos_.y, -10.0f, 10.0f)
        SET_LIGHT("spotPosZ", spotPos_.z, -10.0f, 10.0f)
        SET_LIGHT("spotDirX", spotDir_.x, -1.0f, 1.0f)
        SET_LIGHT("spotDirY", spotDir_.y, -1.0f, 1.0f)
        SET_LIGHT("spotDirZ", spotDir_.z, -1.0f, 1.0f)
        SET_LIGHT("spotIntensity", spotIntensity_, 0.0f, 10.0f)
        SET_LIGHT("spotDistance", spotDistance_, 0.1f, 30.0f)
        SET_LIGHT("spotDecay", spotDecay_, 0.1f, 5.0f)
        if (key == "spotAngleDeg") {
            spotAngleDeg_ = std::clamp(number, 2.0f, 90.0f);
            spotFalloffStartDeg_ = (std::min)(spotFalloffStartDeg_, spotAngleDeg_ - 1.0f);
            ApplyDevelopmentLighting();
            return;
        }
        if (key == "spotFalloffStartDeg") {
            spotFalloffStartDeg_ = std::clamp(number, 1.0f, spotAngleDeg_ - 1.0f);
            ApplyDevelopmentLighting();
            return;
        }
        SET_VALUE("cameraHeightFollowFactor", cameraHeightFollowFactor_, 0.0f, 1.0f)
        SET_VALUE("cameraLookUpFactor", cameraLookUpFactor_, 0.0f, 2.0f)
        if (key == "controlMode") {
            gControlMode = number >= 0.5f ? Player::ControlMode::StarFox : Player::ControlMode::KeyboardAndMouse;
            player_->SetControlMode(gControlMode);
            return;
        }
        if (key == "mouseSensitivity") {
            gMouseSensitivity = std::clamp(number, 0.5f, 2.0f);
            player_->SetMouseSensitivity(gMouseSensitivity);
            return;
        }
        if (key == "cameraFovY") {
            normalFovY_ = std::clamp(number, 0.01f, 3.13f);
            currentFovY_ = normalFovY_;
            camera_->SetFovY(currentFovY_);
            return;
        }
        if (key == "cameraNearClip") { camera_->SetNearClip(std::clamp(number, 0.001f, camera_->GetFarClip() - 0.001f)); return; }
        if (key == "cameraFarClip") { camera_->SetFarClip(std::clamp(number, camera_->GetNearClip() + 0.001f, 10000.0f)); return; }
        if (debugCameraController_->GetDebugMode()) {
            SET_VALUE("cameraX", camera_->GetTranslate().x, -10000.0f, 10000.0f)
            SET_VALUE("cameraY", camera_->GetTranslate().y, -10000.0f, 10000.0f)
            SET_VALUE("cameraZ", camera_->GetTranslate().z, -10000.0f, 10000.0f)
            SET_VALUE("cameraRotX", camera_->GetRotate().x, -6.28f, 6.28f)
            SET_VALUE("cameraRotY", camera_->GetRotate().y, -6.28f, 6.28f)
            SET_VALUE("cameraRotZ", camera_->GetRotate().z, -6.28f, 6.28f)
        }
        if (key == "cameraScaleX" || key == "cameraScaleY" || key == "cameraScaleZ") {
            Vector3 scale = camera_->GetScale();
            if (key == "cameraScaleX") scale.x = std::clamp(number, 0.01f, 10.0f);
            if (key == "cameraScaleY") scale.y = std::clamp(number, 0.01f, 10.0f);
            if (key == "cameraScaleZ") scale.z = std::clamp(number, 0.01f, 10.0f);
            camera_->SetScale(scale);
            return;
        }
#undef SET_LIGHT
#undef SET_VALUE
    }
    if (key == "freeCamera") debugCameraController_->SetDebugMode(enabled);
    else if (key == "returnCamera") debugCameraController_->SetDebugMode(false);
    else if (key == "cameraSpeed") {
        char* end = nullptr;
        const float speed = std::strtof(value.c_str(), &end);
        if (end != value.c_str() && *end == '\0') debugCameraController_->SetMoveSpeed(speed);
    }
    else if (key == "paused") {
        developmentPaused_ = enabled;
        if (!enabled) stepDevelopmentFrame_ = false;
    } else if (key == "step") {
        if (developmentPaused_) stepDevelopmentFrame_ = true;
    } else if (key == "showRail") showRailDebug_ = enabled;
    else if (key == "showCollision") showCollisionDebug_ = enabled;
    else if (key == "showPlayer") showPlayerCollision_ = enabled;
    else if (key == "showEnemy") showEnemyCollision_ = enabled;
    else if (key == "showStage") showStageCollision_ = enabled;
    else if (key == "showBullet") showBulletCollision_ = enabled;
    else if (key == "showIce") showIceJellyfishCollision_ = enabled;
    else if (key == "collisionOverlay") collisionOverlay_ = enabled;
    else if (key == "drawDistance") {
        char* end = nullptr;
        const float distance = std::strtof(value.c_str(), &end);
        if (end != value.c_str() && *end == '\0' && std::isfinite(distance)) {
            collisionDrawDistance_ = std::clamp(distance, 20.0f, 2000.0f);
        }
    } else if (key == "teleportBossArea") {
        railDistance_ = 1750.0f;
    } else if (key == "teleportBoss") {
        if (const StageBoss* boss = GetActiveBoss()) {
            railDistance_ = (std::max)(0.0f, boss->GetPosition().z - 130.0f);
        }
    } else if (key == "teleportJellyfish") {
        if (dynamic_cast<const IceJellyfish*>(GetActiveBoss())) {
            railDistance_ = (std::max)(0.0f, stageSettings_.bossPosition.z - 200.0f);
        }
    }
}
#endif

#if defined(_DEBUG) || defined(ENABLE_DEVELOPMENT_TOOLS)
void GamePlayScene::DrawCollisionDebug()
{
    DebugRenderer* debugRenderer = DebugRenderer::GetInstance();
    const bool previousOverlay = debugRenderer->IsWireframeOverlay();
    debugRenderer->SetWireframeOverlay(collisionOverlay_);
    const Vector3 cameraPosition = camera_->GetTranslate();
    const auto isNearCamera = [&](const Vector3& position, float radius = 0.0f) {
        const float dx = position.x - cameraPosition.x;
        const float dy = position.y - cameraPosition.y;
        const float dz = position.z - cameraPosition.z;
        const float range = collisionDrawDistance_ + (std::max)(0.0f, radius);
        return dx * dx + dy * dy + dz * dz <= range * range;
    };
    if (showEnemyCollision_ && showIceJellyfishCollision_) {
        if (const IceJellyfish* iceJellyfish = dynamic_cast<const IceJellyfish*>(GetActiveBoss())) {
            if (isNearCamera(iceJellyfish->GetPosition(), 100.0f)) {
                iceJellyfish->DrawCollisionDebug();
            }
        }
    }
    constexpr Vector4 kPlayerColor = { 0.0f, 1.0f, 0.0f, 1.0f };
    constexpr Vector4 kEnemyColor = { 1.0f, 0.15f, 0.15f, 1.0f };
    constexpr Vector4 kPlayerBulletColor = { 0.0f, 0.8f, 1.0f, 1.0f };
    constexpr Vector4 kEnemyBulletColor = { 1.0f, 0.85f, 0.0f, 1.0f };
    constexpr Vector4 kStageColliderColor = { 1.0f, 0.0f, 1.0f, 1.0f };
    constexpr float kLineThickness = 3.0f;

    if (showPlayerCollision_ && isNearCamera(player_->GetTranslate(), kPlayerEnemyCollisionRadius)) {
        debugRenderer->AddWireSphere(
            player_->GetTranslate(), kPlayerEnemyCollisionRadius * 0.5f,
            kPlayerColor, kLineThickness);
    }

    if (showBulletCollision_) {
      for (const std::unique_ptr<PlayerBullet>& bullet : player_->GetBullets()) {
        if (bullet->IsAlive() && isNearCamera(bullet->GetPosition(), bullet->GetCollisionRadius())) {
            debugRenderer->AddWireSphere(
                bullet->GetPosition(),
                bullet->GetCollisionRadius(),
                kPlayerBulletColor,
                kLineThickness);
        }
      }
    }

    if (showStageCollision_) {
      for (const std::unique_ptr<Object3d>& levelObject : levelObjects_) {
        const BoxCollider* collider = levelObject->GetCollider();
        if (collider == nullptr) {
            continue;
        }
        const Vector3 size = collider->GetSize();
        const float radius = 0.5f * std::sqrt(size.x * size.x + size.y * size.y + size.z * size.z);
        if (!isNearCamera(collider->GetCenter(), radius)) continue;
        const OBB box = CollisionManager::MakeOBB(
            collider->GetCenter(),
            collider->GetSize(),
            collider->GetRotation());
        debugRenderer->AddWireOBB(
            box.center,
            box.size,
            box.orientation[0],
            box.orientation[1],
            box.orientation[2],
            kStageColliderColor,
            kLineThickness);
      }
    }

    std::vector<EnemyCollisionPart> collisionParts;
    if (showEnemyCollision_) {
      for (const std::unique_ptr<BaseEnemy>& enemy : enemies_) {
        if (enemy->IsDead()) {
            continue;
        }

        collisionParts.clear();
        enemy->GetCollisionParts(collisionParts);
        for (const EnemyCollisionPart& part : collisionParts) {
            if (!isNearCamera(part.position, part.radius)) continue;
            debugRenderer->AddWireSphere(
                part.position,
                part.radius,
                kEnemyColor,
                kLineThickness);
        }
      }
    }

    if (showBulletCollision_) {
      for (const std::unique_ptr<EnemyBullet>& bullet : enemyBulletManager_.GetBullets()) {
        if (bullet->IsAlive() && isNearCamera(bullet->GetPosition(), bullet->GetCollisionRadius())) {
            debugRenderer->AddWireSphere(
                bullet->GetPosition(),
                bullet->GetCollisionRadius() * 0.5f,
                kEnemyBulletColor,
                kLineThickness);
        }
      }
    }

    if (showEnemyCollision_ && GetActiveBoss() != nullptr && !GetActiveBoss()->IsDead()) {
        collisionParts.clear();
        GetActiveBoss()->GetCollisionParts(collisionParts);
        for (const EnemyCollisionPart& part : collisionParts) {
            if (!isNearCamera(part.position, part.radius)) continue;
            debugRenderer->AddWireSphere(
                part.position,
                part.radius,
                kEnemyColor,
                kLineThickness);
        }
    }
    debugRenderer->SetWireframeOverlay(previousOverlay);
}
#endif

void GamePlayScene::Finalize()
{
#if defined(ENABLE_DEVELOPMENT_TOOLS)
    DevelopmentWebPanel::GetInstance().SetScene(nullptr);
#endif
    BaseEnemy::SetBulletManager(nullptr);
    enemyBulletManager_.Clear();
    CollisionManager::GetInstance()->ClearRaycastSphereTargets();
    CollisionManager::GetInstance()->ClearRaycastObbTargets();
    ClearLevelObjects();
    ResetGameplayPostEffects();

    // シーン内で再生していたエフェクトだけを停止する。
    // シェーダーやパイプラインは次回のゲームシーンで再利用する。
    EffectManager::GetInstance()->StopAllEffects();
    EffectManager::GetInstance()->SetCamera(nullptr);
    playerJetHandle_ = kInvalidEffectHandle;
    playerJetSparkHandle_ = kInvalidEffectHandle;

    // SoundManager::GetInstance()->SoundUnload(&bgm);
}

void GamePlayScene::ConfigureGameplayPostEffects(bool isPlayerBoosting)
{
    SceneManager* sceneManager = SceneManager::GetInstance();
    sceneManager->ClearPostEffects();
    sceneManager->AddPostEffect(
        PostEffectType::DepthOutline,
        PostEffectStage::BeforeParticle);
    sceneManager->AddPostEffect(
        PostEffectType::Fog,
        PostEffectStage::BeforeParticle);

    if (isPlayerBoosting) {
        sceneManager->AddPostEffect(
            PostEffectType::RadialBlur,
            PostEffectStage::BeforeParticle);
        sceneManager->AddPostEffect(
            PostEffectType::FocusLine,
            PostEffectStage::BeforeParticle);
        sceneManager->AddPostEffect(
            PostEffectType::Bloom,
            PostEffectStage::BeforeParticle);
    }
}

void GamePlayScene::ApplyDevelopmentLighting()
{
    LightManager* lightManager = LightManager::GetInstance();
    if (lightManager == nullptr) return;
    Vector3 direction = Normalize(lightDir_);
    if (std::abs(direction.x) + std::abs(direction.y) + std::abs(direction.z) < 0.001f) {
        direction = { -0.35f, -0.82f, 0.45f };
    }
    lightManager->SetDirectional(lightColor_, direction, lightEnabled_ ? lightIntensity_ : 0.0f);
    lightManager->SetAmbientColor({ ambientColor_.x, ambientColor_.y, ambientColor_.z });
    lightManager->SetAmbientIntensity(ambientColor_.w);
    lightManager->SetPointRadius(pointRadius_);
    lightManager->SetPointDecay(pointDecay_);
    lightManager->SetPointLight(pointColor_, pointPos_, pointEnabled_ ? pointIntensity_ : 0.0f);
    direction = Normalize(spotDir_);
    if (std::abs(direction.x) + std::abs(direction.y) + std::abs(direction.z) < 0.001f) {
        direction = { -1.0f, 0.0f, 0.0f };
    }
    lightManager->SetSpotLightColor(spotColor_);
    lightManager->SetSpotLightPosition(spotPos_);
    lightManager->SetSpotLightDirection(direction);
    lightManager->SetSpotLightIntensity(spotEnabled_ ? spotIntensity_ : 0.0f);
    lightManager->SetSpotLightDistance(spotDistance_);
    lightManager->SetSpotLightDecay(spotDecay_);
    lightManager->SetSpotLightCosAngle(std::cos(spotAngleDeg_ * std::numbers::pi_v<float> / 180.0f));
    lightManager->SetSpotLightCosFalloffStart(std::cos(
        std::clamp(spotFalloffStartDeg_, 1.0f, (std::max)(1.0f, spotAngleDeg_ - 1.0f)) *
        std::numbers::pi_v<float> / 180.0f));
}

void GamePlayScene::ApplyStageVisualPreset()
{
    LightManager* lightManager = LightManager::GetInstance();

    // Stage03を完成見本とし、青白い主光源と低彩度の環境光で
    // セル陰影の明・中・暗の3段階が安定して読める状態にする。
    if (stageId_ == "stage03") {
        lightManager->SetDirectional(
            { 0.90f, 0.96f, 1.0f, 1.0f },
            Normalize(Vector3 { -0.35f, -0.82f, 0.45f }),
            0.90f);
        lightManager->SetAmbientColor({ 0.40f, 0.52f, 0.68f });
        lightManager->SetAmbientIntensity(0.24f);
    } else {
        lightManager->SetDirectional(
            { 1.0f, 0.97f, 0.90f, 1.0f },
            Normalize(Vector3 { -0.28f, -0.86f, 0.42f }),
            1.0f);
        lightManager->SetAmbientColor({ 0.52f, 0.60f, 0.68f });
        lightManager->SetAmbientIntensity(0.28f);
    }

    lightManager->SetPointRadius(10.0f);
    lightManager->SetPointDecay(1.0f);
    lightManager->SetPointLight(
        { 1.0f, 1.0f, 1.0f, 1.0f },
        { 0.0f, 2.0f, 0.0f },
        0.0f);
    lightManager->SetSpotLightIntensity(0.0f);
}

void GamePlayScene::ResetGameplayPostEffects()
{
    EnemyBullet::SetTimeScale(1.0f);
    justDodgeSlowTimer_ = 0.0f;
    SceneManager::GetInstance()->ClearPostEffects();
    SceneManager::GetInstance()->SetPostEffectCenter({ 0.5f, 0.5f });
    SceneManager::GetInstance()->SetPostEffectKickStrength(0.0f);
    SceneManager::GetInstance()->SetCameraShakeStrength(
        SceneManager::kDefaultCameraShakeStrength);

    boostKickTimer_ = 0.0f;
    boostKickStrength_ = 0.0f;
    wasBoostingForKick_ = false;
    wasPlayerBoosting_ = false;
    smoothedBoostPostEffectCenter_ = { 0.5f, 0.5f };
    cameraShakeTime_ = 0.0f;
    cameraShakeDuration_ = 0.0f;
    cameraShakeStrength_ = 0.0f;
    waterDropEffectTimer_ = 0.0f;
    SceneManager::GetInstance()->SetWaterEffectIntensity(0.0f);
}

void GamePlayScene::UpdateCameraShakePostEffect()
{
    SceneManager* sceneManager = SceneManager::GetInstance();
    if (cameraShakeTime_ <= 0.0f ||
        cameraShakeDuration_ <= 0.0f ||
        cameraShakeStrength_ <= 0.0f) {
        cameraShakeTime_ = 0.0f;
        cameraShakeDuration_ = 0.0f;
        cameraShakeStrength_ = 0.0f;
        sceneManager->SetCameraShakeStrength(0.0f);
        sceneManager->RemovePostEffect(PostEffectType::CameraShake);
        return;
    }

    cameraShakeTime_ -= TimeManager::GetInstance()->GetDeltaTime();
    if (cameraShakeTime_ < 0.0f) {
        cameraShakeTime_ = 0.0f;
    }

    float fadeDuration = cameraShakeDuration_;
    if (fadeDuration > kCameraShakeFadeDuration) {
        fadeDuration = kCameraShakeFadeDuration;
    }

    float fadeRatio = 1.0f;
    if (cameraShakeTime_ < fadeDuration) {
        fadeRatio = cameraShakeTime_ / fadeDuration;
    }

    float currentStrength = cameraShakeStrength_ * fadeRatio;
    sceneManager->SetCameraShakeStrength(currentStrength);
    if (currentStrength > 0.0f) {
        sceneManager->AddPostEffect(
            PostEffectType::CameraShake,
            PostEffectStage::BeforeParticle);
    } else {
        cameraShakeDuration_ = 0.0f;
        cameraShakeStrength_ = 0.0f;
        sceneManager->RemovePostEffect(PostEffectType::CameraShake);
    }
}

void GamePlayScene::StopPlayerEngineEffects()
{
    EffectManager* effectManager = EffectManager::GetInstance();

    if (playerJetHandle_ != kInvalidEffectHandle) {
        effectManager->StopEffect(playerJetHandle_);
        playerJetHandle_ = kInvalidEffectHandle;
    }

    if (playerJetSparkHandle_ != kInvalidEffectHandle) {
        effectManager->StopEffect(playerJetSparkHandle_);
        playerJetSparkHandle_ = kInvalidEffectHandle;
    }

}

void GamePlayScene::UpdateSwarmWaveSpawning()
{
    if (player_ == nullptr) {
        return;
    }
    if (player_->IsDead()) {
        return;
    }
    if (bossController_->IsSpawned()) {
        return;
    }
    const std::vector<float>& waveDistances =
        stageSettings_.swarmWaveDistances;
    if (nextSwarmWaveIndex_ >= waveDistances.size()) {
        return;
    }

    float playerDistance = railDistance_;
    while (nextSwarmWaveIndex_ + 1 < waveDistances.size() &&
           playerDistance >= waveDistances[nextSwarmWaveIndex_ + 1]) {
        nextSwarmWaveIndex_ += 1;
    }

    if (playerDistance < waveDistances[nextSwarmWaveIndex_]) {
        return;
    }

    SwarmFormationType formationType = SwarmFormationType::Spiral;
    size_t formationIndex = nextSwarmWaveIndex_ % 6;
    if (formationIndex == 1) {
        formationType = SwarmFormationType::Wall;
    }
    if (formationIndex == 2) {
        formationType = SwarmFormationType::Glyph;
    }
    if (formationIndex == 3) {
        formationType = SwarmFormationType::Diamond;
    }
    if (formationIndex == 4) {
        formationType = SwarmFormationType::Wave;
    }
    if (formationIndex == 5) {
        formationType = SwarmFormationType::Arrow;
    }

    int32_t travelDirection = 1;
    if (nextSwarmWaveIndex_ % 2 != 0) {
        travelDirection = -1;
    }

    SpawnSwarmWave(formationType, travelDirection);
    nextSwarmWaveIndex_ += 1;
}

void GamePlayScene::SpawnSwarmWave(
    SwarmFormationType formationType,
    int32_t travelDirection)
{
    if (enemyModel_ == nullptr) {
        return;
    }
    if (enemyBulletModel_ == nullptr) {
        return;
    }
    if (player_ == nullptr) {
        return;
    }

    std::shared_ptr<SwarmGroupState> groupState =
        std::make_shared<SwarmGroupState>();
    groupState->totalCount = kSwarmMembersPerWave;
    groupState->activeCount = kSwarmMembersPerWave;

    for (int32_t slotIndex = 0;
         slotIndex < kSwarmMembersPerWave;
         slotIndex += 1) {
        std::unique_ptr<SwarmEnemy> swarmEnemy =
            std::make_unique<SwarmEnemy>();
        swarmEnemy->Initialize(
            enemyModel_,
            enemyBulletModel_,
            player_.get(),
            groupState,
            formationType,
            slotIndex,
            travelDirection);
        enemies_.push_back(std::move(swarmEnemy));
    }
}

void GamePlayScene::LoadEnemyPopData(const LevelData& levelData)
{
    // レベルデータから敵を生成、配置
    for (const auto& enemyData : levelData.enemies) {
        if (enemyData.fileName == "MoveEnemy") {
            std::unique_ptr<MoveEnemy> enemy = std::make_unique<MoveEnemy>();
            enemy->Initialize(enemyModel_, enemyBulletModel_, player_.get());
            enemy->SetPosition(enemyData.translation);
            enemy->SetRotate(enemyData.rotation);

            // デフォルトの移動パターンを設定
            enemy->SetMovePattern(MovePattern::LeftRight);
            enemy->SetAmplitude(8.0f);
            enemy->SetMoveSpeed(2.0f);

            enemies_.push_back(std::move(enemy));
        } else if (enemyData.fileName == "ArmoredEnemy") {
            std::unique_ptr<ArmoredEnemy> enemy =
                std::make_unique<ArmoredEnemy>();
            enemy->Initialize(
                enemyModel_,
                enemyBulletModel_,
                player_.get());
            enemy->SetPosition(enemyData.translation);
            enemy->SetRotate(enemyData.rotation);

            enemies_.push_back(std::move(enemy));
        } else {
            std::unique_ptr<NormalEnemy> enemy = std::make_unique<NormalEnemy>();
            enemy->Initialize(enemyModel_, enemyBulletModel_, player_.get());
            enemy->SetPosition(enemyData.translation);
            enemy->SetRotate(enemyData.rotation);

            enemies_.push_back(std::move(enemy));
        }
    }
}

void GamePlayScene::UpdateBoostKick(bool isPlayerBoosting)
{
    if (isPlayerBoosting && !wasBoostingForKick_) {
        boostKickTimer_ = kBoostKickDuration;
    }

    wasBoostingForKick_ = isPlayerBoosting;

    if (!isPlayerBoosting) {
        boostKickTimer_ = 0.0f;
        boostKickStrength_ = 0.0f;
        SceneManager::GetInstance()->SetPostEffectKickStrength(boostKickStrength_);
        return;
    }

    boostKickStrength_ = 0.0f;
    if (boostKickTimer_ > 0.0f) {
        float normalizedTime = boostKickTimer_ / kBoostKickDuration;
        boostKickStrength_ = normalizedTime * normalizedTime;
        boostKickTimer_ -= TimeManager::GetInstance()->GetDeltaTime();
        if (boostKickTimer_ < 0.0f) {
            boostKickTimer_ = 0.0f;
        }
    }

    SceneManager::GetInstance()->SetPostEffectKickStrength(boostKickStrength_);
}

void GamePlayScene::UpdateBoostPostEffectCenter(float nextRailDistance, bool isPlayerBoosting)
{
    Vector2 targetCenter {};
    targetCenter.x = 0.5f;
    targetCenter.y = 0.5f;

    if (isPlayerBoosting) {
        targetCenter = CalculateBoostPostEffectCenter(nextRailDistance);
        smoothedBoostPostEffectCenter_ = Lerp(smoothedBoostPostEffectCenter_, targetCenter, kBoostPostEffectCenterLerpRate);
    } else {
        smoothedBoostPostEffectCenter_ = targetCenter;
    }

    SceneManager::GetInstance()->SetPostEffectCenter(smoothedBoostPostEffectCenter_);
}

Vector2 GamePlayScene::CalculateBoostPostEffectCenter(float nextRailDistance) const
{
    Vector2 center {};
    center.x = 0.5f;
    center.y = 0.5f;

    if (camera_ == nullptr) {
        return center;
    }

    if (rail_ == nullptr) {
        return center;
    }

    if (player_ == nullptr) {
        return center;
    }

    float clientWidth = static_cast<float>(WinApp::GetInstance()->GetClientWidth());
    float clientHeight = static_cast<float>(WinApp::GetInstance()->GetClientHeight());

    if (clientWidth <= 0.0f) {
        clientWidth = static_cast<float>(WinApp::kClientWidth);
    }

    if (clientHeight <= 0.0f) {
        clientHeight = static_cast<float>(WinApp::kClientHeight);
    }

    float vanishPointDistance = nextRailDistance + kBoostPostEffectVanishPointDistance;
    float totalLength = rail_->GetTotalLength();
    if (vanishPointDistance > totalLength) {
        vanishPointDistance = totalLength;
    }

    Vector3 vanishPointPosition = rail_->GetPositionByDistance(vanishPointDistance);
    Vector2 vanishPointScreen = camera_->WorldToScreen(vanishPointPosition);
    Vector2 vanishPointCenter = ScreenPositionToPostEffectCenter(vanishPointScreen, clientWidth, clientHeight);

    Vector2 playerScreen = camera_->WorldToScreen(player_->GetTranslate());
    Vector2 playerCenter = ScreenPositionToPostEffectCenter(playerScreen, clientWidth, clientHeight);

    center.x =
        0.5f * kBoostPostEffectBaseWeight +
        vanishPointCenter.x * kBoostPostEffectVanishPointWeight +
        playerCenter.x * kBoostPostEffectPlayerWeight;
    center.y =
        0.5f * kBoostPostEffectBaseWeight +
        vanishPointCenter.y * kBoostPostEffectVanishPointWeight +
        playerCenter.y * kBoostPostEffectPlayerWeight;

    center.x = std::clamp(center.x, kBoostPostEffectCenterMin, kBoostPostEffectCenterMax);
    center.y = std::clamp(center.y, kBoostPostEffectCenterMin, kBoostPostEffectCenterMax);

    return center;
}

void GamePlayScene::CreateLevelObjects(const LevelData& levelData)
{
    hasCameraPoint_ = false;
    cameraPointObject_ = {};
    cameraPointLerpTime_ = 0.0f;

    for (const LevelData::ObjectData& objData : levelData.objects) {
        if (objData.disabled) {
            continue;
        }

        if (objData.cameraPoint.exists) {
            cameraPointObject_ = objData;
            hasCameraPoint_ = true;
            cameraPointLerpTime_ = 0.0f;
        }

        if (objData.type == "MESH") {
            if (objData.fileName.empty()) {
                continue;
            }
            ModelManager::GetInstance()->Load(objData.fileName);

            std::unique_ptr<Object3d> levelObject = std::make_unique<Object3d>();
            levelObject->Initialize(Object3dManager::GetInstance());
            levelObject->SetModel(objData.fileName);
            levelObject->SetTranslate(objData.translation);
            levelObject->SetRotate(objData.rotation);
            levelObject->SetScale(objData.scale);

            const bool isIceModel = objData.fileName.starts_with("Environment/Ice/") ||
                objData.fileName == "IceSpike.obj";
            if (stageId_ == "stage03" && isIceModel) {
                levelObject->SetColor({ 0.82f, 0.94f, 1.0f, 1.0f });
                levelObject->SetShadingMode(MaterialShadingMode::Ice);
                const char* iceMaterial = "resources/Shaders/Object3D/StageIceSpire";
                if (objData.fileName == "Environment/Ice/ice_island.obj") {
                    iceMaterial = "resources/Shaders/Object3D/StageIceIsland";
                } else if (objData.fileName == "Environment/Ice/ice_boulder.obj") {
                    iceMaterial = "resources/Shaders/Object3D/StageIceBoulder";
                } else if (objData.fileName == "Environment/Ice/ice_slab.obj") {
                    iceMaterial = "resources/Shaders/Object3D/StageIceSlab";
                } else if (objData.fileName == "Environment/Ice/ice_arch.obj") {
                    iceMaterial = "resources/Shaders/Object3D/StageIceArch";
                } else if (objData.fileName == "Environment/Ice/crystal.obj") {
                    iceMaterial = "resources/Shaders/Object3D/StageIceCrystal";
                }
                levelObject->SetMaterial(iceMaterial);
                levelObject->GetMaterial()->shininess = 0.0f;
                levelObject->SetEnableEnvironmentMap(false);
            }

            if (objData.gimmick.exists) {
                levelObject->SetGimmick(objData.gimmick);
            }

            if (objData.destructible.exists) {
                levelObject->SetColor({ 0.30f, 0.20f, 0.14f, 1.0f });
                destructibleLevelObjects_.push_back({
                    levelObject.get(),
                    (std::max)(objData.destructible.hp, 1.0f),
                    false
                });
            }

            if (objData.hazard.exists) {
                levelObject->SetCollisionDamage(objData.hazard.damage);
                if (objData.hazard.type == "LASER") {
                    levelObject->SetColor({ 1.0f, 0.03f, 0.02f, 1.0f });
                    levelObject->SetEnableLighting(false);
                }
            }

            if (objData.trigger.exists &&
                (objData.trigger.type == "WIND" ||
                 objData.trigger.type == "GRAVITY")) {
                levelObject->SetColor({ 0.15f, 0.75f, 1.0f, 0.65f });
                levelObject->SetEnableLighting(false);
            }

            if (objData.trigger.exists) {
                stageTriggers_.push_back({
                    levelObject.get(),
                    objData.trigger.type,
                    objData.trigger.name,
                    objData.trigger.center,
                    objData.trigger.size,
                    objData.trigger.force,
                    false
                });
            }

            if (objData.collider.exists) {
                if (objData.collider.type == "BOX") {
                    std::unique_ptr<BoxCollider> collider = std::make_unique<BoxCollider>();
                    Vector3 center = {
                        objData.translation.x + objData.collider.center.x,
                        objData.translation.y + objData.collider.center.y,
                        objData.translation.z + objData.collider.center.z
                    };
                    collider->SetCenter(center);
                    collider->SetSize(objData.collider.size);

                    BoxCollider* registeredCollider = CollisionManager::GetInstance()->RegisterCollider(std::move(collider));
                    levelObject->SetCollider(registeredCollider);
                }
            }

            levelObjects_.push_back(std::move(levelObject));
        }
        else if (objData.type == "EnemySpawn") {
            if (objData.fileName == "MoveEnemy") {
                std::unique_ptr<MoveEnemy> enemy = std::make_unique<MoveEnemy>();
                enemy->Initialize(enemyModel_, enemyBulletModel_, player_.get());
                enemy->SetPosition(objData.translation);
                enemy->SetRotate(objData.rotation);

                enemy->SetMovePattern(MovePattern::LeftRight);
                enemy->SetAmplitude(8.0f);
                enemy->SetMoveSpeed(2.0f);

                if (objData.patrolRoute.exists) {
                    enemy->SetPatrolWaypoints(objData.patrolRoute.waypoints);
                }

                enemies_.push_back(std::move(enemy));
            } else if (objData.fileName == "ArmoredEnemy") {
                std::unique_ptr<ArmoredEnemy> enemy =
                    std::make_unique<ArmoredEnemy>();
                enemy->Initialize(
                    enemyModel_,
                    enemyBulletModel_,
                    player_.get());
                enemy->SetPosition(objData.translation);
                enemy->SetRotate(objData.rotation);

                if (objData.patrolRoute.exists) {
                    enemy->SetPatrolWaypoints(
                        objData.patrolRoute.waypoints);
                }

                enemies_.push_back(std::move(enemy));
            } else {
                std::unique_ptr<NormalEnemy> enemy = std::make_unique<NormalEnemy>();
                enemy->Initialize(enemyModel_, enemyBulletModel_, player_.get());
                enemy->SetPosition(objData.translation);
                enemy->SetRotate(objData.rotation);

                if (objData.patrolRoute.exists) {
                    enemy->SetPatrolWaypoints(objData.patrolRoute.waypoints);
                }

                enemies_.push_back(std::move(enemy));
            }
        }
    }
}

void GamePlayScene::HotReloadLevel()
{
    ClearLevelObjects();
    enemies_.clear();
    enemyBulletManager_.Clear();
    nextSwarmWaveIndex_ = 0;

    Vector3 playerStartPos = { 0.0f, 0.0f, 0.0f };
    Vector3 playerStartRot = { 0.0f, 0.0f, 0.0f };

    LevelDataLoader levelDataLoader;
    LevelData newLevelData =
        levelDataLoader.Load(stageSettings_.layoutFile);

    if (!newLevelData.playerSpawns.empty()) {
        const LevelData::PlayerSpawnData& spawn = newLevelData.playerSpawns[0];
        playerStartPos = spawn.translation;
        playerStartRot = spawn.rotation;
    }

    player_->SetTranslate(playerStartPos);
    player_->SetRotate(playerStartRot);
    player_->SetRailFrame(playerStartPos, { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f });

    CreateLevelObjects(newLevelData);
}

void GamePlayScene::ClearLevelObjects()
{
    stageTriggers_.clear();
    destructibleLevelObjects_.clear();
    for (std::unique_ptr<Object3d>& obj : levelObjects_) {
        if (obj->GetCollider() != nullptr) {
            CollisionManager::GetInstance()->UnregisterCollider(obj->GetCollider());
            obj->SetCollider(nullptr);
        }
    }
    levelObjects_.clear();
}

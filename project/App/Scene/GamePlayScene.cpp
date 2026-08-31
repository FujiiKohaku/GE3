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
#include <algorithm>
#include <cmath>

namespace {
Player::ControlMode gControlMode = Player::ControlMode::KeyboardAndMouse;
float gMouseSensitivity = 1.0f;

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
    /// ポストエフェクト初期化
    SceneManager::GetInstance()->SetPostEffectType(PostEffectType::DepthOutline);
    SceneManager::GetInstance()->AddPostEffect(
        PostEffectType::Bloom,
        PostEffectStage::BeforeParticle);
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

    LightManager::GetInstance()->SetDirectional({ 1, 1, 1, 1 }, { 0, -1, 0 }, 1.0f);

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
    weaponHudBgSprite_ = std::make_unique<Sprite>();
    weaponHudBgSprite_->Initialize(SpriteManager::GetInstance(), "resources/Textures/white.png");
    weaponHudBgSprite_->SetSize({ 280.0f, 76.0f });
    weaponHudBgSprite_->SetAnchorPoint({ 0.0f, 1.0f });
    weaponHudBgSprite_->SetPosition({ 24.0f, WinApp::GetInstance()->kClientHeight - 24.0f });
    weaponHudBgSprite_->SetColor({ 0.04f, 0.07f, 0.12f, 0.82f });
    weaponHudBgSprite_->Update();

    weaponHudLabelText_ = std::make_unique<Text>();
    weaponHudLabelText_->Initialize(kDefaultFont);
    weaponHudLabelText_->SetText("WEAPON");
    weaponHudLabelText_->SetPosition({ 40.0f, WinApp::GetInstance()->kClientHeight - 92.0f });
    weaponHudLabelText_->SetFontSize(16.0f);
    weaponHudLabelText_->SetColor({ 0.35f, 0.85f, 1.0f, 1.0f });
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
    playerHpBgSprite_ = std::make_unique<Sprite>();
    playerHpBgSprite_->Initialize(SpriteManager::GetInstance(), "resources/Textures/white.png");
    playerHpBgSprite_->SetSize({ 220.0f, 22.0f });
    playerHpBgSprite_->SetAnchorPoint({ 1.0f, 0.0f });
    playerHpBgSprite_->SetPosition({ WinApp::GetInstance()->kClientWidth - 30.0f, 40.0f });
    playerHpBgSprite_->SetColor({ 0.08f, 0.08f, 0.12f, 0.85f });
    playerHpBgSprite_->Update();

    playerHpBarSprite_ = std::make_unique<Sprite>();
    playerHpBarSprite_->Initialize(SpriteManager::GetInstance(), "resources/Textures/white.png");
    playerHpBarSprite_->SetMaterial("resources/Shaders/Sprite/HealthBar");
    playerHpBarSprite_->SetSize({ 220.0f, 22.0f });
    playerHpBarSprite_->SetAnchorPoint({ 1.0f, 0.0f });
    playerHpBarSprite_->SetPosition({ WinApp::GetInstance()->kClientWidth - 30.0f, 40.0f });
    playerHpBarSprite_->SetColor({ 0.20f, 0.85f, 0.40f, 0.95f });
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

    bossHeadHpBgSprite_ = std::make_unique<Sprite>();
    bossHeadHpBgSprite_->Initialize(SpriteManager::GetInstance(), "resources/Textures/white.png");
    bossHeadHpBgSprite_->SetSize({ bossHpBarWidth, 14.0f });
    bossHeadHpBgSprite_->SetAnchorPoint({ 0.0f, 0.0f });
    bossHeadHpBgSprite_->SetPosition({ bossHpBarLeft, 65.0f });
    bossHeadHpBgSprite_->SetColor({ 0.06f, 0.12f, 0.18f, 0.90f });
    bossHeadHpBgSprite_->Update();

    bossHeadHpBarSprite_ = std::make_unique<Sprite>();
    bossHeadHpBarSprite_->Initialize(SpriteManager::GetInstance(), "resources/Textures/white.png");
    bossHeadHpBarSprite_->SetMaterial("resources/Shaders/Sprite/HealthBar");
    bossHeadHpBarSprite_->SetSize({ bossHpBarWidth, 14.0f });
    bossHeadHpBarSprite_->SetAnchorPoint({ 0.0f, 0.0f });
    bossHeadHpBarSprite_->SetPosition({ bossHpBarLeft, 65.0f });
    bossHeadHpBarSprite_->SetColor({ 0.20f, 0.60f, 1.00f, 0.95f });
    bossHeadHpBarSprite_->Update();

    bossBodyHpBgSprite_ = std::make_unique<Sprite>();
    bossBodyHpBgSprite_->Initialize(SpriteManager::GetInstance(), "resources/Textures/white.png");
    bossBodyHpBgSprite_->SetSize({ bossHpBarWidth, 14.0f });
    bossBodyHpBgSprite_->SetAnchorPoint({ 0.0f, 0.0f });
    bossBodyHpBgSprite_->SetPosition({ bossHpBarLeft, 91.0f });
    bossBodyHpBgSprite_->SetColor({ 0.18f, 0.06f, 0.06f, 0.90f });
    bossBodyHpBgSprite_->Update();

    bossBodyHpBarSprite_ = std::make_unique<Sprite>();
    bossBodyHpBarSprite_->Initialize(SpriteManager::GetInstance(), "resources/Textures/white.png");
    bossBodyHpBarSprite_->SetMaterial("resources/Shaders/Sprite/HealthBar");
    bossBodyHpBarSprite_->SetSize({ bossHpBarWidth, 14.0f });
    bossBodyHpBarSprite_->SetAnchorPoint({ 0.0f, 0.0f });
    bossBodyHpBarSprite_->SetPosition({ bossHpBarLeft, 91.0f });
    bossBodyHpBarSprite_->SetColor({ 1.00f, 0.20f, 0.20f, 0.95f });
    bossBodyHpBarSprite_->Update();

    bossNameText_ = std::make_unique<Text>();
    bossNameText_->Initialize(kDefaultFont);
    bossNameText_->SetText(
        stageSettings_.bossType == "AngerBlock" ? "BOSS: ANGER" : "BOSS: FEAR WORM");
    bossNameText_->SetPosition({ bossHudCenterX, 12.0f });
    bossNameText_->SetAnchorPoint({ 0.5f, 0.0f });
    bossNameText_->SetFontSize(20.0f);
    bossNameText_->SetColor({ 1.0f, 0.25f, 0.25f, 1.0f });
    bossNameText_->Update();

    bossHeadHpText_ = std::make_unique<Text>();
    bossHeadHpText_->Initialize(kDefaultFont);
    bossHeadHpText_->SetText(
        stageSettings_.bossType == "AngerBlock" ? "ANGER CORE" : "HEAD CORE");
    bossHeadHpText_->SetPosition({ bossHpBarLeft - 12.0f, 60.0f });
    bossHeadHpText_->SetAnchorPoint({ 1.0f, 0.0f });
    bossHeadHpText_->SetFontSize(14.0f);
    bossHeadHpText_->SetColor({ 0.55f, 0.80f, 1.0f, 1.0f });
    bossHeadHpText_->Update();

    bossBodyHpText_ = std::make_unique<Text>();
    bossBodyHpText_->Initialize(kDefaultFont);
    bossBodyHpText_->SetText(
        stageSettings_.bossType == "AngerBlock" ? "FISTS" : "BODY SHIELD");
    bossBodyHpText_->SetPosition({ bossHpBarLeft - 12.0f, 86.0f });
    bossBodyHpText_->SetAnchorPoint({ 1.0f, 0.0f });
    bossBodyHpText_->SetFontSize(14.0f);
    bossBodyHpText_->SetColor({ 1.0f, 0.55f, 0.55f, 1.0f });
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
            floorObj_->SetColor({ 0.10f, 0.24f, 0.36f, 1.0f });
            floorObj_->GetMaterial()->shininess = 0.0f;
            floorObj_->SetEnableEnvironmentMap(false);
        }
    }

    Logger::Log("GamePlayScene::Initialize: Completed successfully");
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
    if (input != nullptr && input->IsKeyTrigger(DIK_TAB)) {
        isPaused_ = !isPaused_;
    }

    if (isPaused_) {
        SceneManager::GetInstance()->SetCameraShakeStrength(0.0f);
        SceneManager::GetInstance()->RemovePostEffect(PostEffectType::CameraShake);

        // ポーズ中は背景画面にガウスぼかし（GaussianFilter）、モノクロ白黒化（GrayScale）、SFホログラム走査線（CyberScanline）をトリプル適用
        SceneManager::GetInstance()->AddPostEffect(PostEffectType::GaussianFilter,PostEffectStage::BeforeParticle);
        SceneManager::GetInstance()->AddPostEffect(PostEffectType::GrayScale,PostEffectStage::BeforeParticle);
        SceneManager::GetInstance()->AddPostEffect(PostEffectType::CyberScanline,PostEffectStage::BeforeParticle);

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
        return;
    }
    if (Input::GetInstance()->IsKeyTrigger(DIK_F5) || Input::GetInstance()->IsKeyTrigger(DIK_R)) {
        HotReloadLevel();
    }

    // 時間停止中はゲーム世界を更新しない。
    // チュートリアルUIは、今後この判定より前でUnscaledDeltaTimeを使って更新する。
    if (TimeManager::GetInstance()->GetDeltaTime() <= 0.0f) {
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
    // Vキーを押すとボス登場前の座標（Z = 1450.0f）まで一瞬でワープ！
    if (stageSettings_.bossType != "None" && Input::GetInstance()->IsKeyTrigger(DIK_V)) {
        railDistance_ = (std::max)(
            0.0f,
            stageSettings_.bossSpawnDistance - 400.0f);
        if (player_) {
            Vector3 pPos = player_->GetTranslate();
            player_->SetTranslate({ pPos.x, pPos.y, railDistance_ });
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
    if (bossController_->DidSpawnThisFrame()) {
        bossNoiseFadeTimer_ = 4.5f;
    }
    if (bossController_->DidExtendRailThisFrame() && oceanSurface_ != nullptr) {
        oceanSurface_->SetLength(rail_->GetTotalLength());
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

    // 入力インスタンスの取得
    if (input != nullptr) {
        if (input->IsKeyTrigger(DIK_L)) {
            isRandomPostEffect_ = !isRandomPostEffect_;
            hasRandomPostEffectToggle_ = true;
        }
    }

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

    // ポストエフェクトの切り替え
    if (hasRandomPostEffectToggle_) {
        if (isRandomPostEffect_) {
            SceneManager::GetInstance()->SetPostEffectType(PostEffectType::Random);
        } else {
            SceneManager::GetInstance()->SetPostEffectType(PostEffectType::Copy);
            SceneManager::GetInstance()->AddPostEffect(
                PostEffectType::Bloom,
                PostEffectStage::BeforeParticle);
        }
    } else {
        if (isPlayerBoosting) {
            SceneManager::GetInstance()->ClearPostEffects();
            SceneManager::GetInstance()->AddPostEffect(PostEffectType::Fog,PostEffectStage::BeforeParticle);
            SceneManager::GetInstance()->AddPostEffect(PostEffectType::RadialBlur,PostEffectStage::BeforeParticle);
            SceneManager::GetInstance()->AddPostEffect(PostEffectType::FocusLine,PostEffectStage::BeforeParticle);
            SceneManager::GetInstance()->AddPostEffect(
                PostEffectType::ChromaticAberration,
                PostEffectStage::BeforeParticle);
            SceneManager::GetInstance()->AddPostEffect(
                PostEffectType::Bloom,
                PostEffectStage::BeforeParticle);
        } else if (GetActiveBoss() != nullptr && !GetActiveBoss()->IsDead()) {
            // ボス戦中: 3Dボスの輝度境界を強調する LuminanceBasedOutline (5点加点) を適用！
            SceneManager::GetInstance()->SetPostEffectType(PostEffectType::LuminanceBasedOutline);
            SceneManager::GetInstance()->AddPostEffect(
                PostEffectType::Bloom,
                PostEffectStage::BeforeParticle);
        } else {
            // 通常時: 深度ベースのアウトライン (DepthOutline: 8点加点)
            SceneManager::GetInstance()->SetPostEffectType(PostEffectType::DepthOutline);
            SceneManager::GetInstance()->AddPostEffect(
                PostEffectType::Bloom,
                PostEffectStage::BeforeParticle);
        }
    }

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

    // -------------------------------------------------
    // ミニガン連射時の「銃身熱気カゲロウ (HeatHaze)」演出
    // -------------------------------------------------
    if (player_ && player_->GetHeatRatio() > 0.01f) {
        SceneManager::GetInstance()->SetVignetteStrength(player_->GetHeatRatio());
        SceneManager::GetInstance()->AddPostEffect(
            PostEffectType::HeatHaze,
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
    // HP ≦ 5 ピンチ時: 画面端の不規則ビキビキガラスひび割れ (GlassCrack)
    // -------------------------------------------------
    if (player_ && player_->GetCurrentHp() <= 5) {
        SceneManager::GetInstance()->AddPostEffect(
            PostEffectType::GlassCrack,
            PostEffectStage::BeforeParticle);
    }

    // -------------------------------------------------
    // 加点要素: Random (4点)
    // 途切れ一切無しの完全シームレスノイズ＆たっぷり4.5秒間のロングフェードアウト
    // -------------------------------------------------
    float noiseIntensity = 0.0f;

    // (A) ボス登場前予兆ノイズ (Z = 1450 〜 1850)
    const float bossWarningStart = (std::max)(
        0.0f,
        stageSettings_.bossSpawnDistance - 400.0f);
    if (stageSettings_.bossType != "None" && !bossController_->IsSpawned() && player_ && railDistance_ >= bossWarningStart) {
        float playerDistance = railDistance_;
        float warningLength =
            stageSettings_.bossSpawnDistance - bossWarningStart;
        float progress = warningLength > 0.0f
            ? (playerDistance - bossWarningStart) / warningLength
            : 1.0f;
        if (progress > 1.0f) progress = 1.0f;
        noiseIntensity = 0.30f + 0.70f * progress;
    }
    // (B) ボス登場後のロングフェードアウトノイズ (たっぷり4.5秒間かけて非常にゆっくり消えていく)
    else if (bossNoiseFadeTimer_ > 0.0f) {
        bossNoiseFadeTimer_ -= TimeManager::GetInstance()->GetDeltaTime();
        if (bossNoiseFadeTimer_ < 0.0f) bossNoiseFadeTimer_ = 0.0f;

        float fadeProgress = bossNoiseFadeTimer_ / 4.5f;
        noiseIntensity = fadeProgress;
    }

    // 途切れが絶対に発生しないよう、ノイズ強度がわずかでもある場合は100%確実に適用！
    if (noiseIntensity > 0.001f) {
        SceneManager::GetInstance()->SetVignetteStrength(noiseIntensity);
        SceneManager::GetInstance()->AddPostEffect(
            PostEffectType::Random,
            PostEffectStage::BeforeParticle);
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

        // 残りHP量に応じてバーの色を変化（緑 -> 黄色 -> 赤）
        if (currentHp <= 3) {
            playerHpBarSprite_->SetColor({ 0.95f, 0.15f, 0.15f, 0.95f });
        } else if (hpRatio < 0.45f) {
            playerHpBarSprite_->SetColor({ 0.95f, 0.70f, 0.15f, 0.95f });
        } else {
            playerHpBarSprite_->SetColor({ 0.20f, 0.85f, 0.40f, 0.95f });
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
#ifdef USE_IMGUI

    // player_->DrawImGui();

    // ==================================
    // Lighting Panel（ライト操作パネル）
    // ==================================
    ImGui::Begin("Lighting Control");

    // ---- ライトの ON / OFF ----
    static bool lightEnabled = false;
    ImGui::Checkbox("Enable Light", &lightEnabled);

    // ---- ライトの色 ----
    static Vector4 lightColor = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    ImGui::ColorEdit3("Light Color", (float*)&lightColor);

    // ---- 明るさ（強さ） ----
    static float lightIntensity = 1.0f;
    ImGui::SliderFloat("Intensity", &lightIntensity, 0.0f, 5.0f);

    // ---- 光の向き ----
    static Vector3 lightDir = { 0.0f, -1.0f, 0.0f };
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

    static Vector4 ambientColor = Vector4(1.0f, 1.0f, 1.0f, 0.25f);

    // ---- リセットボタン（向きだけ元に戻す）---
    if (ImGui::Button("Reset Direction")) {
        lightDir = { 0.0f, -1.0f, 0.0f };
    }

    ImGui::SameLine();

    // ---- ライトを完全初期化 ----
    if (ImGui::Button("Reset Light")) {
        lightEnabled = true;
        lightColor = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
        lightIntensity = 1.0f;
        lightDir = { 0.0f, -1.0f, 0.0f };
        ambientColor = Vector4(1.0f, 1.0f, 1.0f, 0.25f);
    }

    ImGui::ColorEdit3("Ambient Color", &ambientColor.x);
    ImGui::SliderFloat("Ambient Intensity", &ambientColor.w, 0.0f, 1.0f);
    LightManager::GetInstance()->SetAmbientColor({ ambientColor.x, ambientColor.y, ambientColor.z });
    LightManager::GetInstance()->SetAmbientIntensity(ambientColor.w);

    ImGui::End();

    // Point Light コントロール
    ImGui::Begin("Point Light Control");
    static bool pointEnabled = true;
    ImGui::Checkbox("Enable Point Light", &pointEnabled);

    static Vector4 pointColor = { 1, 1, 1, 1 };
    ImGui::ColorEdit3("Point Color", (float*)&pointColor);

    static Vector3 pointPos = { 0.0f, 2.0f, 0.0f };
    ImGui::SliderFloat3("Point Position", &pointPos.x, -10.0f, 10.0f);

    static float pointIntensity = 1.0f;
    ImGui::SliderFloat("Point Intensity", &pointIntensity, 0.0f, 5.0f);

    static float pointRadius = 10.0f;
    static float pointDecay = 1.0f;
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
    static bool spotEnabled = true;
    ImGui::Checkbox("Enable Spot Light", &spotEnabled);

    // 色
    static Vector4 spotColor = { 1, 1, 1, 1 };
    ImGui::ColorEdit3("Spot Color", (float*)&spotColor);

    // 位置
    static Vector3 spotPos = { 0.0f, 0.0f, 0.0f };
    ImGui::SliderFloat3("Spot Position", &spotPos.x, -10.0f, 10.0f);

    // 方向
    static Vector3 spotDir = { -1.0f, 0.0f, 0.0f };
    ImGui::SliderFloat3("Spot Direction", &spotDir.x, -1.0f, 1.0f);
    Vector3 normalizedSpotDir = Normalize(spotDir);

    // 強さ
    static float spotIntensity = 4.0f;
    ImGui::SliderFloat("Spot Intensity", &spotIntensity, 0.0f, 10.0f);

    // 距離・減衰
    static float spotDistance = 7.0f;
    static float spotDecay = 2.0f;
    ImGui::SliderFloat("Spot Distance", &spotDistance, 0.1f, 30.0f);
    ImGui::SliderFloat("Spot Decay", &spotDecay, 0.1f, 5.0f);

    // 角度（度数で操作し、cos に変換）
    static float spotAngleDeg = 60.0f;
    static float spotFalloffStartDeg = 30.0f;

    ImGui::SliderFloat("Spot Angle (deg)", &spotAngleDeg, 1.0f, 90.0f);
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

    ImGui::End();

    // 反映
#else
    LightManager::GetInstance()->SetDirectional(
        { 1.0f, 1.0f, 1.0f, 1.0f },
        { 0.0f, -1.0f, 0.0f },
        0.0f);

    LightManager::GetInstance()->SetPointRadius(10.0f);
    LightManager::GetInstance()->SetPointDecay(1.0f);
    LightManager::GetInstance()->SetPointLight(
        { 1.0f, 1.0f, 1.0f, 1.0f },
        { 0.0f, 2.0f, 0.0f },
        0.0f);

    LightManager::GetInstance()->SetAmbientColor({ 1.0f, 1.0f, 1.0f });
    LightManager::GetInstance()->SetAmbientIntensity(0.25f);

    LightManager* lightManager = LightManager::GetInstance();
    lightManager->SetSpotLightColor({ 1.0f, 1.0f, 1.0f, 1.0f });
    lightManager->SetSpotLightPosition({ 0.0f, 0.0f, 0.0f });
    lightManager->SetSpotLightDirection({ -1.0f, 0.0f, 0.0f });
    lightManager->SetSpotLightIntensity(4.0f);
    lightManager->SetSpotLightDistance(7.0f);
    lightManager->SetSpotLightDecay(2.0f);
    lightManager->SetSpotLightCosAngle(0.5f);
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
    // カメラポイント補間の適用
    if (hasCameraPoint_) {
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

    // 1. デバッグカメラコントローラーの更新
    debugCameraController_->Update();

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
    if (input != nullptr) {
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
    LightManager::GetInstance()->Bind(DirectXCommon::GetInstance()->GetCommandList());

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
        LightManager::GetInstance()->Bind(DirectXCommon::GetInstance()->GetCommandList());
    }
    for (std::unique_ptr<BaseEnemy>& enemy : enemies_) {
        enemy->Draw();
    }
    enemyBulletManager_.Draw();

    if (GetActiveBoss() != nullptr) {
        GetActiveBoss()->Draw();
    }

#ifdef _DEBUG
    rail_->DrawDebug();
    DrawCollisionDebug();
#endif

    //----------------------
    // スキニング
    //----------------------
    SkinningObject3dManager::GetInstance()->PreDraw();
    LightManager::GetInstance()->Bind(DirectXCommon::GetInstance()->GetCommandList());
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
        recoveryItem.object->SetColor({ 0.20f, 1.0f, 0.35f, 1.0f });
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

    // 画面右側のプレイヤーHPゲージ（背景スプライト＆HPバー）の描画
    if (weaponHudBgSprite_) weaponHudBgSprite_->Draw();
    if (playerHpBgSprite_) playerHpBgSprite_->Draw();
    if (playerHpBarSprite_) playerHpBarSprite_->Draw();

    if (GetActiveBoss() != nullptr && !GetActiveBoss()->IsDeathSequenceFinished()) {
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
            ImGui::Text("BOSS: %s", stageSettings_.bossType == "AngerBlock" ? "ANGER" : "FEAR WORM");
            ImGui::PopStyleColor();

            // 1. 頭部HPバー (ネオンブルー)
            float headFraction = GetActiveBoss()->GetHeadHpFraction();
            ImGui::Text("HEAD CORE  ");
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.2f, 0.6f, 1.0f, 1.0f)); // ネオンブルー
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.1f, 0.2f, 0.3f, 0.4f));       // 暗い青背景
            ImGui::ProgressBar(headFraction, ImVec2(-1, 14.0f), "");
            ImGui::PopStyleColor(2);

            // 2. 胴体HPバー (ネオンレッド + 胴体数に応じた9分割の区切り線)
            float bodyFraction = GetActiveBoss()->GetBodyHpFraction();
            ImGui::Text("BODY SHIELD");
            ImGui::SameLine();
            
            ImVec2 barPosMin = ImGui::GetCursorScreenPos();
            
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(1.0f, 0.2f, 0.2f, 1.0f)); // ネオンレッド
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.3f, 0.1f, 0.1f, 0.4f));       // 暗い赤背景
            ImGui::ProgressBar(bodyFraction, ImVec2(-1, 14.0f), "");
            ImGui::PopStyleColor(2);

            // 直前に描画したProgressBarの領域を取得して、9分割(8本の縦線)で区切る
            ImVec2 barPosMax = ImGui::GetItemRectMax();
            float barWidth = barPosMax.x - barPosMin.x;
            constexpr int kSegmentDivisions = 9;
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

#ifdef _DEBUG
void GamePlayScene::DrawCollisionDebug()
{
    DebugRenderer* debugRenderer = DebugRenderer::GetInstance();
    constexpr Vector4 kPlayerColor = { 0.0f, 1.0f, 0.0f, 1.0f };
    constexpr Vector4 kEnemyColor = { 1.0f, 0.15f, 0.15f, 1.0f };
    constexpr Vector4 kPlayerBulletColor = { 0.0f, 0.8f, 1.0f, 1.0f };
    constexpr Vector4 kEnemyBulletColor = { 1.0f, 0.85f, 0.0f, 1.0f };
    constexpr Vector4 kStageColliderColor = { 1.0f, 0.0f, 1.0f, 1.0f };
    constexpr float kLineThickness = 2.0f;

    debugRenderer->AddWireSphere(
        player_->GetTranslate(),
        kPlayerEnemyCollisionRadius * 0.5f,
        kPlayerColor,
        kLineThickness);

    for (const std::unique_ptr<PlayerBullet>& bullet : player_->GetBullets()) {
        if (bullet->IsAlive()) {
            debugRenderer->AddWireSphere(
                bullet->GetPosition(),
                bullet->GetCollisionRadius(),
                kPlayerBulletColor,
                kLineThickness);
        }
    }

    for (const std::unique_ptr<Object3d>& levelObject : levelObjects_) {
        const BoxCollider* collider = levelObject->GetCollider();
        if (collider == nullptr) {
            continue;
        }
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

    std::vector<EnemyCollisionPart> collisionParts;
    for (const std::unique_ptr<BaseEnemy>& enemy : enemies_) {
        if (enemy->IsDead()) {
            continue;
        }

        collisionParts.clear();
        enemy->GetCollisionParts(collisionParts);
        for (const EnemyCollisionPart& part : collisionParts) {
            debugRenderer->AddWireSphere(
                part.position,
                part.radius,
                kEnemyColor,
                kLineThickness);
        }
    }

    for (const std::unique_ptr<EnemyBullet>& bullet : enemyBulletManager_.GetBullets()) {
        if (bullet->IsAlive()) {
            debugRenderer->AddWireSphere(
                bullet->GetPosition(),
                bullet->GetCollisionRadius() * 0.5f,
                kEnemyBulletColor,
                kLineThickness);
        }
    }

    if (GetActiveBoss() == nullptr || GetActiveBoss()->IsDead()) {
        return;
    }

    collisionParts.clear();
    GetActiveBoss()->GetCollisionParts(collisionParts);
    for (const EnemyCollisionPart& part : collisionParts) {
        debugRenderer->AddWireSphere(
            part.position,
            part.radius,
            kEnemyColor,
            kLineThickness);
    }

}
#endif

void GamePlayScene::Finalize()
{
    BaseEnemy::SetBulletManager(nullptr);
    enemyBulletManager_.Clear();
    CollisionManager::GetInstance()->ClearRaycastSphereTargets();
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

            if (stageId_ == "stage03" && objData.fileName.starts_with("Environment/Ice/")) {
                levelObject->SetColor({ 0.90f, 0.96f, 1.0f, 1.0f });
                // Diffuse ice facets: white lit faces and blue side faces, without glare.
                levelObject->GetMaterial()->enableLighting = 2;
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

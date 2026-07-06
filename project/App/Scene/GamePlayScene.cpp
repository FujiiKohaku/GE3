#include "GamePlayScene.h"
#include "Engine/3D/SphereObject.h"
#include "Engine/Animation/AnimationLoder.h"
#include "Engine/CollisionManager/CollisionManager.h"
#include "Engine/Effect/EffectManager.h"
#include "Engine/Light/LightManager.h"
#include "Engine/audio/SoundManager.h"
#include <numbers>

#include "SceneManager.h"

#include "../externals/json.hpp"
#include "Engine/PostEffect/PostEffectType.h"
#include <fstream>
#include <string_view>

#include "../../Engine/LevelEditor/LevelDataLoader.h"

#include "ClearScene.h"
#include "GameOverScene.h"
#include "Engine/Debug/DebugRenderer.h"
#include "Engine/Logger/Logger.h"
#include "Engine/Input/Input.h"
#include <cmath>

namespace {
constexpr float kPlayerEnemyCollisionRadius = 2.0f;
constexpr float kPlayerBulletEnemyCollisionRadius = 2.0f;

float LengthSquared(const Vector3& value)
{
    return value.x * value.x + value.y * value.y + value.z * value.z;
}

bool IsZeroVector(const Vector3& value)
{
    return LengthSquared(value) < 0.000001f;
}

Vector3 Cross(const Vector3& a, const Vector3& b)
{
    Vector3 result {};
    result.x = a.y * b.z - a.z * b.y;
    result.y = a.z * b.x - a.x * b.z;
    result.z = a.x * b.y - a.y * b.x;
    return result;
}
}

void GamePlayScene::Initialize()
{

    editorManager_ = std::make_unique<EditorManager>();
    editorManager_->Initialize();
    sceneObjectManager_ = std::make_unique<SceneObjectManager>();
    rail_ = std::make_unique<Rail>();
    rail_->Initialize();
    rail_->AddPoint({ 0.0f, 0.0f, 0.0f });
    rail_->AddPoint({ 0.0f, 0.0f, 50.0f });
    rail_->AddPoint({ 0.0f, 0.0f, 100.0f });
    rail_->AddPoint({ 0.0f, 0.0f, 150.0f });
    rail_->AddPoint({ 0.0f, 0.0f, 200.0f });
    rail_->AddPoint({ 0.0f, 0.0f, 250.0f });
    rail_->AddPoint({ 0.0f, 0.0f, 300.0f });
    rail_->AddPoint({ 0.0f, 0.0f, 350.0f });
    rail_->AddPoint({ 0.0f, 0.0f, 400.0f });
    rail_->AddPoint({ 0.0f, 0.0f, 450.0f });
    rail_->AddPoint({ 0.0f, 0.0f, 500.0f });
    rail_->AddPoint({ 0.0f, 0.0f, 550.0f });
    rail_->AddPoint({ 0.0f, 0.0f, 600.0f });
    rail_->AddPoint({ 0.0f, 0.0f, 650.0f });
    rail_->AddPoint({ 0.0f, 0.0f, 700.0f });
    rail_->AddPoint({ 0.0f, 0.0f, 750.0f });
    rail_->AddPoint({ 0.0f, 0.0f, 800.0f });
    rail_->AddPoint({ 0.0f, 0.0f, 850.0f });
    rail_->AddPoint({ 0.0f, 0.0f, 900.0f });
    rail_->AddPoint({ 0.0f, 0.0f, 950.0f });
    rail_->AddPoint({ 0.0f, 0.0f, 1000.0f });
    rail_->AddPoint({ 0.0f, 0.0f, 1050.0f });
    rail_->AddPoint({ 0.0f, 0.0f, 1100.0f });
    rail_->AddPoint({ 0.0f, 0.0f, 1150.0f });
    rail_->AddPoint({ 0.0f, 0.0f, 1200.0f });
    rail_->AddPoint({ 0.0f, 0.0f, 1250.0f });
    rail_->AddPoint({ 0.0f, 0.0f, 1300.0f });
    rail_->AddPoint({ 0.0f, 0.0f, 1350.0f });
    rail_->AddPoint({ 0.0f, 0.0f, 1400.0f });
    rail_->AddPoint({ 0.0f, 0.0f, 1450.0f });
    rail_->AddPoint({ 0.0f, 0.0f, 1500.0f });
    rail_->AddPoint({ 0.0f, 0.0f, 1550.0f });
    rail_->AddPoint({ 0.0f, 0.0f, 1600.0f });
    rail_->AddPoint({ 0.0f, 0.0f, 1650.0f });
    rail_->AddPoint({ 0.0f, 0.0f, 1700.0f });
    rail_->AddPoint({ 0.0f, 0.0f, 1750.0f });
    rail_->AddPoint({ 0.0f, 0.0f, 1800.0f });
    rail_->AddPoint({ 0.0f, 0.0f, 1850.0f });
    rail_->AddPoint({ 0.0f, 0.0f, 1900.0f });
    rail_->AddPoint({ 0.0f, 0.0f, 1950.0f });
    rail_->AddPoint({ 0.0f, 0.0f, 2000.0f });
    /// ポストエフェクト初期化
    SceneManager::GetInstance()->SetPostEffectType(PostEffectType::DepthOutline);
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
    EffectManager::GetInstance()->Initialize(DirectXCommon::GetInstance(), SrvManager::GetInstance(), camera_.get());
    // =================================================
    // SkinningObject3d
    // =================================================

    TextureManager::GetInstance()->LoadTexture("resources/Textures/BaseColor_Cube.png");
    TextureManager::GetInstance()->LoadTexture("resources/Textures/uvChecker.png");
    TextureManager::GetInstance()->LoadTexture("resources/Textures/skybox.dds");
    TextureManager::GetInstance()->LoadTexture("resources/Textures/rostock_laage_airport_4k.dds");
    TextureManager::GetInstance()->LoadTexture("resources/Textures/aim.png"); // AiMスプライチE

    // nodeLoad
    ModelManager::GetInstance()->Load("Characters/Enemy/Drone/dolone.obj");
    ModelManager::GetInstance()->Load("Characters/Animation/SneakWalk/sneakWalk.gltf");
    ModelManager::GetInstance()->Load("Debug/Samples/AnimatedCube/AnimatedCube.gltf");
    Model* playerModel = ModelManager::GetInstance()->Load("Characters/Player/AirPlane/AirPlane.obj");

    Model* enemyModel_ = ModelManager::GetInstance()->Load("Weapons/Star/star.obj");
    Model* enemyBulletModel_ = ModelManager::GetInstance()->Load("Weapons/Star/star.obj");
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
    Logger::Log("GamePlayScene::Initialize: Loading uvChecker texture");
    TextureManager::GetInstance()->LoadTexture("resources/Textures/uvChecker.png");
    Logger::Log("GamePlayScene::Initialize: Allocating skyBox");
    skyBox_ = std::make_unique<SkyBox>();
    Logger::Log("GamePlayScene::Initialize: Initializing skyBox");
    skyBox_->Initialize(DirectXCommon::GetInstance());
    Logger::Log("GamePlayScene::Initialize: Setting skyBox texture");
    skyBox_->SetTexture("resources/Textures/skybox.dds");
    Logger::Log("GamePlayScene::Initialize: skyBox initialization finished");

    // =================================================
    // Playerクラス
    // =================================================
    Logger::Log("GamePlayScene::Initialize: Starting player initialization");
    player_ = std::make_unique<Player>();
    player_->Initialize(playerModel);
    player_->SetCamera(camera_.get());
    player_->SetDebugCameraController(debugCameraController_.get());
    player_->SetTranslate({ 0.0f, 0.0f, 0.0f });
    Logger::Log("GamePlayScene::Initialize: player initialized successfully");
    playerJetHandle_ = EffectManager::GetInstance()->AttachEffect("Jet", player_);
    wasPlayerBoosting_ = false;

    /*LevelDataLoader levelDataLoader;
    LevelData levelData = levelDataLoader.Load("resources/Scenes/stage01.json");*/

    /* for (const LevelData::ObjectData& objectData : levelData.objects) {
         if (objectData.type != "MESH") {
             continue;
         }

         if (objectData.fileName.empty()) {
             continue;
         }

         ModelManager::GetInstance()->Load(objectData.fileName);

         std::unique_ptr<Object3d> levelObject = std::make_unique<Object3d>();
         levelObject->Initialize(Object3dManager::GetInstance());
         levelObject->SetModel(objectData.fileName);
         levelObject->SetTranslate(objectData.translation);
         levelObject->SetRotate(objectData.rotation);
         levelObject->SetScale(objectData.scale);
         levelObjects_.push_back(std::move(levelObject));
     }*/
    // editorManager_->SetSelectedObject(terrain_);

    editorManager_->SetSceneObjectManager(sceneObjectManager_.get());

    const Vector3 enemyPositions[] = {
        { -14.0f, -3.0f, 120.0f },
        { 10.0f, 5.0f, 160.0f },
        { 18.0f, -6.0f, 210.0f },
        { -7.0f, 7.0f, 260.0f },
        { 14.0f, 0.0f, 310.0f },
        { -18.0f, 4.0f, 360.0f },
        { 4.0f, -7.0f, 410.0f },
        { 17.0f, 6.0f, 460.0f },
        { -12.0f, 1.0f, 510.0f },
        { 7.0f, -5.0f, 560.0f },
        { -20.0f, -2.0f, 610.0f },
        { 13.0f, 7.0f, 660.0f },
        { -5.0f, 4.0f, 710.0f },
        { 20.0f, -4.0f, 760.0f },
        { -15.0f, 6.0f, 810.0f },
        { 9.0f, 0.0f, 860.0f },
        { -2.0f, -7.0f, 900.0f },
        { 16.0f, 3.0f, 940.0f },
        { -10.0f, -4.0f, 980.0f },
        { 5.0f, 6.0f, 1000.0f },
        { -19.0f, 0.0f, 450.0f },
        { 19.0f, -1.0f, 720.0f },
    };

    for (const Vector3& enemyPosition : enemyPositions) {

        std::unique_ptr<NormalEnemy> enemy = std::make_unique<NormalEnemy>();

        enemy->Initialize(enemyModel_, enemyBulletModel_, player_.get());

        enemy->SetPosition(enemyPosition);

        enemies_.push_back(std::move(enemy));
    }

    CollisionManager::GetInstance()->SetEnemies(&enemies_);
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

    if (IsZeroVector(forward)) {
        forward = Normalize(nextPosition - railPosition);
    }

    if (IsZeroVector(forward)) {
        forward = Normalize(railPosition - previousPosition);
    }

    if (IsZeroVector(forward)) {
        forward = { 0.0f, 0.0f, 1.0f };
    }

    return forward;
}

void GamePlayScene::CalculateRailBasis(const Vector3& forward, Vector3& right, Vector3& up) const
{
    Vector3 normalizedForward = Normalize(forward);
    if (IsZeroVector(normalizedForward)) {
        normalizedForward = { 0.0f, 0.0f, 1.0f };
    }

    Vector3 referenceUp = { 0.0f, 1.0f, 0.0f };
    right = Normalize(Cross(referenceUp, normalizedForward));

    if (IsZeroVector(right)) {
        Vector3 referenceForward = { 0.0f, 0.0f, 1.0f };
        right = Normalize(Cross(normalizedForward, referenceForward));
    }

    if (IsZeroVector(right)) {
        right = { 1.0f, 0.0f, 0.0f };
    }

    up = Normalize(Cross(normalizedForward, right));

    if (IsZeroVector(up)) {
        up = referenceUp;
    }
}

void GamePlayScene::Update()
{
    // レール自体の更新
    rail_->Update();

    // 敵全体の更新
    for (std::unique_ptr<BaseEnemy>& enemy : enemies_) {
        enemy->Update();
    }

    // プレイヤーのZ座標を取得
    float playerZ = player_->GetTranslate().z;

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
    Input* input = Input::GetInstance();
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

    // 2. プレイヤーの位置・回転などのワールドトランスフォームの確定
    UpdatePlayerTransform(currentPosition, railRight, railUp, forward);

    // 進行距離を更新
    railDistance_ = nextRailDistance;

    // 3. 描画用カメラと仮想カメラの同期・更新
    UpdateCamera(currentPosition, forward, railRight, railUp, nextRailDistance, input);

    // 4. マウス左クリックによる弾の発射処理
    ProcessPlayerShooting(input);

#ifdef _DEBUG
    if (isFirstFrame) {
        Logger::Log("GamePlayScene::Update: First frame completed successfully");
        isFirstFrame = false;
    }
#endif

    // デバッグ用の進行方向ライン描画 (緑色)
    DebugRenderer::GetInstance()->AddLine(
        currentPosition,
        currentPosition + forward * 20.0f,
        { 0.0f, 1.0f, 0.0f, 1.0f },
        3.0f);

    // プレイヤーのブースト状態に応じたエフェクト制御
    const bool isPlayerBoosting = player_->IsBoosting();
    Vector3 boostLinePosition = player_->GetTranslate();
    boostLinePosition.y += 0.2f;
    boostLinePosition.z += 2.4f;

    if (isPlayerBoosting != wasPlayerBoosting_) {
        EffectManager::GetInstance()->StopEffect(playerJetHandle_);

        const char* jetEffectName = "Jet";
        if (isPlayerBoosting) {
            jetEffectName = "JetBoost";
        }
        playerJetHandle_ = EffectManager::GetInstance()->AttachEffect(jetEffectName, player_);

        if (isPlayerBoosting) {
            boostLineHandle_ = EffectManager::GetInstance()->AttachEffect("BoostLine", player_);
            EffectManager::GetInstance()->SetEffectPosition(boostLineHandle_, boostLinePosition);
        } else {
            EffectManager::GetInstance()->StopEffect(boostLineHandle_);
            boostLineHandle_ = kInvalidEffectHandle;
        }
        wasPlayerBoosting_ = isPlayerBoosting;
    }

    if (isPlayerBoosting && boostLineHandle_ != kInvalidEffectHandle) {
        EffectManager::GetInstance()->SetEffectPosition(boostLineHandle_, boostLinePosition);
    }

    // ポストエフェクトの切り替え
    PostEffectType postEffectType = PostEffectType::DepthOutline;
    if (isPlayerBoosting) {
        postEffectType = PostEffectType::RadialBlur;
    }

    if (hasRandomPostEffectToggle_) {
        if (isRandomPostEffect_) {
            SceneManager::GetInstance()->SetPostEffectType(PostEffectType::Random);
        } else {
            SceneManager::GetInstance()->SetPostEffectType(PostEffectType::Copy);
        }
    } else {
        SceneManager::GetInstance()->SetPostEffectType(postEffectType);
    }

    // レティクル（AimSprite）のスクリーン位置更新
    aimSprite_->SetPosition(player_->GetAimScreenPosition());
    aimSprite_->Update();

    // スカイボックスの更新
    skyBox_->Update(camera_.get());

    // 各種マネージャー、オブジェクト、コリジョンの更新
    EffectManager::GetInstance()->Update();
    sceneObjectManager_->Update();
    animationActor_->Update(1.0f / 60.0f);
    
    // コリジョン判定の実行
    CheckCollision();

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
    }

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

    float pI = pointEnabled ? pointIntensity : 0.0f;
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
    float sI = spotEnabled ? spotIntensity : 0.0f;

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
    outNextDistance = railDistance_ + railSpeed_;
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
    // プレイヤーにレール情報の最新のフレーム（座標、右方向、上方向、前方向）を伝える
    player_->SetRailFrame(currentPosition, railRight, railUp, forward);
    
    // プレイヤーの内部座標（移動制限など）を更新
    player_->Update();

    // 進行方向に合わせてプレイヤーの回転を適用
    if (forward.x != 0.0f || forward.y != 0.0f || forward.z != 0.0f) {
        float horizontalLength = std::sqrt(forward.x * forward.x + forward.z * forward.z);
        Vector3 playerRotate {};
        playerRotate.x = -std::atan2(forward.y, horizontalLength);
        playerRotate.y = -std::atan2(forward.x, forward.z);
        playerRotate.z = 0.0f;
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
            if (!IsZeroVector(lerpedForward)) {
                cameraForward = Normalize(lerpedForward);
            }
        }

        smoothedCameraForward_ = cameraForward;

        // 描画用カメラのターゲット座標（Lerp前）
        Vector3 targetDrawCameraPosition = currentPosition - cameraForward * kCameraBackwardOffset;
        targetDrawCameraPosition.y += kCameraUpwardOffset;

        // 描画用カメラのターゲット注視点（Lerp前）
        Vector3 targetLookAheadPositionDraw = rail_->GetPositionByDistance(nextRailDistance + cameraLookAheadDistance_);

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

void GamePlayScene::ProcessPlayerShooting(Input* input)
{
    if (input != nullptr) {
        if (input->IsMouseTrigger(0)) {
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

    Object3dManager::GetInstance()->PreDraw();
    LightManager::GetInstance()->Bind(DirectXCommon::GetInstance()->GetCommandList());

    // Object3dManager::GetInstance()->SetGlowPSO();
    // Object3dManager::GetInstance()->SetNormalPSO();
    // Object3dManager::GetInstance()->SetBlendMode(kBlendModeMultiply);
    // terrain_->Draw();
    // for (std::unique_ptr<Object3d>& levelObject : levelObjects_) {
    //     levelObject->Draw();
    // }
    player_->Draw();
    sceneObjectManager_->Draw();

    for (std::unique_ptr<BaseEnemy>& enemy : enemies_) {
        enemy->Draw();
    }

    rail_->DrawDebug();

    //----------------------
    // スキニング
    //----------------------
    SkinningObject3dManager::GetInstance()->PreDraw();
    LightManager::GetInstance()->Bind(DirectXCommon::GetInstance()->GetCommandList()); // ここでもう一回バインドしなぁE��ぁE��なぁE
                                                                                        // animationSkin00_->Draw();
    animationActor_->Draw();
}

void GamePlayScene::DrawParticle()
{
    EffectManager::GetInstance()->PreDraw();
    EffectManager::GetInstance()->Draw();
}

void GamePlayScene::Draw2D()
{
    SpriteManager::GetInstance()->PreDraw();
    // testSprite_->Draw();
    aimSprite_->Draw();
}

void GamePlayScene::DrawImGui()
{
#ifdef USE_IMGUI
    camera_->DrawImGui();
    editorManager_->DrawImGui();
    editorManager_->DrawGizmo(camera_.get());
    player_->DrawImGui();
#endif
}

void GamePlayScene::CheckCollision()
{
    for (std::unique_ptr<BaseEnemy>& enemy : enemies_) {

        Vector3 difference = enemy->GetPosition() - player_->GetTranslate();

        float distance = sqrtf(difference.x * difference.x + difference.y * difference.y + difference.z * difference.z);

        float collisionRadius = kPlayerEnemyCollisionRadius;

        if (distance <= collisionRadius) {

            OutputDebugStringA("Player Hit Enemy\n");
        }
    }

    for (const std::unique_ptr<PlayerBullet>& bullet : player_->GetBullets()) {

        for (std::unique_ptr<BaseEnemy>& enemy : enemies_) {

            if (enemy->IsDead()) {
                continue;
            }

            Vector3 difference = enemy->GetPosition() - bullet->GetPosition();

            float distance = sqrtf(difference.x * difference.x + difference.y * difference.y + difference.z * difference.z);

            float collisionRadius = kPlayerBulletEnemyCollisionRadius;

            if (distance <= collisionRadius) {

                OutputDebugStringA("PlayerBullet Hit Enemy\n");

                bullet->OnHitEnemy(enemy->GetPosition());

                enemy->ApplyDamage(static_cast<float>(bullet->GetDamage()));

                bullet->SetDead();

                break;
            }
        }
    }
    // 死んだ敵の中から「NormalEnemy」だけを選んで安�Eに削除する
    std::erase_if(enemies_, [](const std::unique_ptr<BaseEnemy>& enemy) {
        // 1. 敵が死んでいるかをチェックする
        if (enemy->IsDead()) {

            // 2. 敵が死んでいる場合、さらにそれが NormalEnemy かどうかをチェックする
            // dynamic_cast を使って、enemy が NormalEnemy に安全にキャストできるかを確認する
            if (dynamic_cast<NormalEnemy*>(enemy.get()) != nullptr) {

                // 3. 敵が NormalEnemy であれば、true を返して削除する
                return true;
            }
        }

        // 4. 敵が死んでいない、または NormalEnemy でない場合は、false を返して削除しない
        return false;
    });

    // 敵の弾とプレイヤーの当たり判定
    for (std::unique_ptr<BaseEnemy>& enemy : enemies_) {
        if (enemy->IsDead()) {
            continue;
        }

        for (const std::unique_ptr<EnemyBullet>& enemyBullet : enemy->GetBullets()) {
            if (!enemyBullet->IsAlive()) {
                continue;
            }

            Vector3 difference = enemyBullet->GetPosition() - player_->GetTranslate();
            float distance = sqrtf(difference.x * difference.x + difference.y * difference.y + difference.z * difference.z);
            float collisionRadius = enemyBullet->GetCollisionRadius();

            if (distance <= collisionRadius) {
                OutputDebugStringA("EnemyBullet Hit Player\n");

                enemyBullet->OnHitPlayer(player_->GetTranslate());

                if (player_->ApplyDamage(enemyBullet->GetDamage())) {
                    EffectManager::GetInstance()->PlayEffect("DamageHit", player_->GetTranslate());

                    if (player_->IsDead()) {
                        SceneManager::GetInstance()->SetNextScene(std::make_unique<GameOverScene>());
                    }
                }
                enemyBullet->SetDead();
                // 1発の弾で複数回ダメージを受けないように、当たったらすぐに弾を無効化する
            }
        }
    }
}

void GamePlayScene::Finalize()
{
    CollisionManager::GetInstance()->SetEnemies(nullptr);

    EffectManager::GetInstance()->StopEffect(playerJetHandle_);
    playerJetHandle_ = kInvalidEffectHandle;
    EffectManager::GetInstance()->StopEffect(boostLineHandle_);
    boostLineHandle_ = kInvalidEffectHandle;
    EffectManager::Finalize();

    // SoundManager::GetInstance()->SoundUnload(&bgm);
}

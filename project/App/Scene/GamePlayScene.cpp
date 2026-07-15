#include "GamePlayScene.h"
#include "App/Game/Enemy/MoveEnemy/MoveEnemy.h"
#include "Engine/3D/SphereObject.h"
#include "Engine/Animation/AnimationLoder.h"
#include "Engine/CollisionManager/CollisionManager.h"
#include "Engine/Effect/EffectManager.h"
#include "Engine/Light/LightManager.h"
#include "Engine/math/MathStruct.h"
#include "Engine/audio/SoundManager.h"
#include <numbers>
#include <cstdlib>

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
constexpr float kPlayerEnemyCollisionRadius = 4.0f;
constexpr float kPlayerBulletEnemyCollisionRadius = 4.0f;
constexpr float kBoostPostEffectBaseWeight = 0.40f;
constexpr float kBoostPostEffectVanishPointWeight = 0.30f;
constexpr float kBoostPostEffectPlayerWeight = 0.30f;
constexpr float kBoostPostEffectCenterMin = 0.28f;
constexpr float kBoostPostEffectCenterMax = 0.72f;
constexpr float kBoostPostEffectVanishPointDistance = 150.0f;
constexpr float kBoostPostEffectCenterLerpRate = 0.1f;
constexpr float kBoostKickDuration = 0.2f;
constexpr float kBoostKickFrameTime = 1.0f / 60.0f;
constexpr float kBoostKickFovAdd = 0.1f;

float ClampFloat(float value, float minValue, float maxValue)
{
    if (value < minValue) {
        value = minValue;
    }

    if (value > maxValue) {
        value = maxValue;
    }

    return value;
}

Vector2 LerpVector2(const Vector2& start, const Vector2& end, float rate)
{
    Vector2 result {};
    result.x = start.x + (end.x - start.x) * rate;
    result.y = start.y + (end.y - start.y) * rate;
    return result;
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

bool IsZeroVector(const Vector3& value)
{
    return Vector3LengthSquared(value) < 0.000001f;
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
    for (float z = 0.0f; z <= 3600.0f; z += 50.0f) {
        rail_->AddPoint({ 0.0f, 0.0f, z });
    }
    /// ポストエフェクト初期化
    SceneManager::GetInstance()->SetPostEffectType(PostEffectType::DepthOutline);
    SceneManager::GetInstance()->AddPostEffect(PostEffectType::Bloom);
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
    TextureManager::GetInstance()->LoadTexture("resources/Textures/skybox.dds");
    TextureManager::GetInstance()->LoadTexture("resources/Textures/rostock_laage_airport_4k.dds");
    TextureManager::GetInstance()->LoadTexture("resources/Textures/aim.png"); // AiMスプライチE

    // nodeLoad
    ModelManager::GetInstance()->Load("Characters/Enemy/Drone/dolone.obj");
    ModelManager::GetInstance()->Load("Characters/Animation/SneakWalk/sneakWalk.gltf");
    ModelManager::GetInstance()->Load("Debug/Samples/AnimatedCube/AnimatedCube.gltf");
    Model* playerModel = ModelManager::GetInstance()->Load("fish/fish.obj");

    // エネミー・弾モデル
    enemyModel_ = ModelManager::GetInstance()->Load("Debug/baikinMusi/baikinMusi.obj");
    enemyBulletModel_ = ModelManager::GetInstance()->Load("Debug/block/block.obj");
    fearWormEnemyModel_ = ModelManager::GetInstance()->Load("Debug/Sphere/sphere.obj");
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
    playerJetSparkHandle_ = EffectManager::GetInstance()->AttachEffect("JetSpark", player_);
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

    LoadEnemyPopData();

    CollisionManager::GetInstance()->SetEnemies(&enemies_);
    CollisionManager::GetInstance()->SetBoss(nullptr);

    // floorの初期化
    Model* floorModel = ModelManager::GetInstance()->CreatePlane("resources/Textures/floor_dirt_gemini.jpg", 100.0f, 360.0f);
    floorObj_ = std::make_unique<Object3d>();
    floorObj_->Initialize(Object3dManager::GetInstance());
    floorObj_->SetModel(floorModel);
    floorObj_->SetTranslate({ 0.0f, -30.0f, 1800.0f });
    floorObj_->SetRotate({ std::numbers::pi_v<float> / 2.0f, 0.0f, 0.0f });
    floorObj_->SetScale({ 1000.0f, 3600.0f, 1.0f });

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

    // ボス出現処理
    if (!isBossSpawned_ && playerZ >= 1850.0f) {
        activeBoss_ = std::make_unique<FearWormEnemy>();
        activeBoss_->Initialize(
            fearWormEnemyModel_,
            enemyBulletModel_,
            player_.get());
        activeBoss_->SetPosition({ 0.0f, 2.0f, 1850.0f });
        CollisionManager::GetInstance()->SetBoss(activeBoss_.get());
        isBossSpawned_ = true;
    }

    // プレイヤーのHP減少検知による被弾カメラシェイク
    if (player_) {
        static int lastPlayerHp = player_->GetCurrentHp();
        int currentPlayerHp = player_->GetCurrentHp();
        if (currentPlayerHp < lastPlayerHp) {
            cameraShakeTime_ = 0.35f;
            cameraShakeDuration_ = 0.35f;
            cameraShakeIntensity_ = 0.6f;
        }
        lastPlayerHp = currentPlayerHp;
    }

    // ボスの更新
    if (activeBoss_) {
        bool wasMadMode = activeBoss_->IsMadModeActive();

        activeBoss_->Update();

        // 発狂モードに入った瞬間を検知してカメラシェイクを開始する
        if (activeBoss_->IsMadModeActive() && !wasMadMode) {
            cameraShakeTime_ = 7.0f;
            cameraShakeDuration_ = 7.0f;
            cameraShakeIntensity_ = 0.8f;
        }

        // ビーム被弾中のカメラ微振動
        if (activeBoss_->IsBeamHittingPlayer()) {
            if (cameraShakeTime_ < 0.1f || cameraShakeIntensity_ < 0.15f) {
                cameraShakeTime_ = 0.1f;
                cameraShakeDuration_ = 0.1f;
                cameraShakeIntensity_ = 0.15f;
            }
        }
    }

    // ボス撃破でクリアシーンへ遷移
    if (activeBoss_ && activeBoss_->IsDead()) {
        CollisionManager::GetInstance()->SetBoss(nullptr);

        // 死亡演出(頭部の落下回転)が完了するまでボスのUpdateを回し続ける
        activeBoss_->Update();
        EffectManager::GetInstance()->Update();

        if (activeBoss_->IsDeathSequenceFinished()) {
            ResetGameplayPostEffects();
            SceneManager::GetInstance()->SetNextScene(std::make_unique<ClearScene>());
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
    const bool isPlayerBoosting = player_->IsBoosting();
    UpdateBoostKick(isPlayerBoosting);

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
    Vector3 boostLinePosition = player_->GetTranslate();
    boostLinePosition.y += 0.2f;
    boostLinePosition.z += 2.4f;

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

    UpdateBoostPostEffectCenter(nextRailDistance, isPlayerBoosting);

    // ポストエフェクトの切り替え
    if (hasRandomPostEffectToggle_) {
        if (isRandomPostEffect_) {
            SceneManager::GetInstance()->SetPostEffectType(PostEffectType::Random);
        } else {
            SceneManager::GetInstance()->SetPostEffectType(PostEffectType::Copy);
            SceneManager::GetInstance()->AddPostEffect(PostEffectType::Bloom);
        }
    } else {
        if (isPlayerBoosting) {
            SceneManager::GetInstance()->ClearPostEffects();
            SceneManager::GetInstance()->AddPostEffect(PostEffectType::Fog);
            SceneManager::GetInstance()->AddPostEffect(PostEffectType::RadialBlur);
            SceneManager::GetInstance()->AddPostEffect(PostEffectType::FocusLine);
            SceneManager::GetInstance()->AddPostEffect(PostEffectType::Bloom);
        } else {
            SceneManager::GetInstance()->SetPostEffectType(PostEffectType::DepthOutline);
            SceneManager::GetInstance()->AddPostEffect(PostEffectType::Bloom);
        }
    }

    // レティクル（AimSprite）のスクリーン位置更新
    aimSprite_->SetPosition(player_->GetAimScreenPosition());
    aimSprite_->Update();

    // スカイボックスの更新
    skyBox_->Update(camera_.get());

    // 各種マネージャー、オブジェクト、コリジョンの更新
    EffectManager::GetInstance()->Update();
    sceneObjectManager_->Update();
    if (floorObj_) {
        floorObj_->Update();
    }

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
        // プレイヤーのレール相対移動量を取得
        Vector3 playerRailOffset = player_->GetRailOffset();

        Vector3 targetDrawCameraPosition = currentPosition - cameraForward * kCameraBackwardOffset;
        targetDrawCameraPosition += railUp * (kCameraUpwardOffset + playerRailOffset.y * cameraHeightFollowFactor_);

        // 描画用カメラのターゲット注視点（Lerp前）
        Vector3 targetLookAheadPositionDraw = rail_->GetPositionByDistance(nextRailDistance + cameraLookAheadDistance_);
        targetLookAheadPositionDraw += railUp * (playerRailOffset.y * cameraLookUpFactor_);

        // 遅延追従（Lerp）の適用
        if (hasCameraFollowState_) {
            smoothedCameraPosition_ = Lerp(smoothedCameraPosition_, targetDrawCameraPosition, cameraFollowLerpRate_);
            smoothedLookAheadPosition_ = Lerp(smoothedLookAheadPosition_, targetLookAheadPositionDraw, cameraFollowLerpRate_);
        } else {
            smoothedCameraPosition_ = targetDrawCameraPosition;
            smoothedLookAheadPosition_ = targetLookAheadPositionDraw;
            hasCameraFollowState_ = true;
        }

        // カメラシェイクの適用
        Vector3 shakeOffset = { 0.0f, 0.0f, 0.0f };
        if (cameraShakeTime_ > 0.0f) {
            cameraShakeTime_ -= 1.0f / 60.0f;
            if (cameraShakeTime_ < 0.0f) {
                cameraShakeTime_ = 0.0f;
            }

            // 最初の2.0秒間（残り7.0s〜5.0s）は最大強度を維持、後半5.0秒間で徐々に減衰
            float intensity = cameraShakeIntensity_;
            if (cameraShakeTime_ < 5.0f) {
                float ratio = cameraShakeTime_ / 5.0f; // 1.0 -> 0.0
                intensity = cameraShakeIntensity_ * ratio;
            }

            // 簡易乱数による3軸方向の揺れ量
            float rx = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) * 2.0f - 1.0f;
            float ry = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) * 2.0f - 1.0f;
            float rz = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) * 2.0f - 1.0f;

            shakeOffset.x = rx * intensity;
            shakeOffset.y = ry * intensity;
            shakeOffset.z = rz * intensity;
        }

        // cameraを行列再計算のためにLookAt設定
        camera_->LookAt(smoothedCameraPosition_ + shakeOffset, smoothedLookAheadPosition_ + shakeOffset);
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
    if (floorObj_) {
        floorObj_->Draw();
    }

    for (std::unique_ptr<BaseEnemy>& enemy : enemies_) {
        enemy->Draw();
    }

    if (activeBoss_) {
        activeBoss_->Draw();
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
    if (!activeBoss_ || !activeBoss_->IsDead()) {
        aimSprite_->Draw();
    }
}

void GamePlayScene::DrawImGui()
{
#ifdef USE_IMGUI
    // ボス出現時、画面上部中央にスタイリッシュな2本の横長HPバーをHUD風にオーバーレイ表示する
    if (activeBoss_ && !activeBoss_->IsDeathSequenceFinished()) {
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
            ImGui::Text("BOSS: FEAR WORM");
            ImGui::PopStyleColor();

            // 1. 頭部HPバー (ネオンブルー)
            float headFraction = activeBoss_->GetHeadHpFraction();
            ImGui::Text("HEAD CORE  ");
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.2f, 0.6f, 1.0f, 1.0f)); // ネオンブルー
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.1f, 0.2f, 0.3f, 0.4f));       // 暗い青背景
            ImGui::ProgressBar(headFraction, ImVec2(-1, 14.0f), "");
            ImGui::PopStyleColor(2);

            // 2. 胴体HPバー (ネオンレッド + 胴体数に応じた9分割の区切り線)
            float bodyFraction = activeBoss_->GetBodyHpFraction();
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
    if (activeBoss_) {
        ImGui::Text("Boss Z: %.2f", activeBoss_->GetPosition().z);
        if (ImGui::Button("Teleport to Boss")) {
            float bossZ = activeBoss_->GetPosition().z;
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
    std::vector<EnemyCollisionPart> enemyCollisionParts;

    for (std::unique_ptr<BaseEnemy>& enemy : enemies_) {
        if (enemy->IsDead()) {
            continue;
        }

        enemyCollisionParts.clear();
        enemy->GetCollisionParts(enemyCollisionParts);

        for (const EnemyCollisionPart& part : enemyCollisionParts) {
            Vector3 difference = part.position - player_->GetTranslate();

            float distance = Vector3Length(difference);

            float collisionRadius = part.radius + kPlayerEnemyCollisionRadius * 0.5f;

            if (distance <= collisionRadius) {

                OutputDebugStringA("Player Hit Enemy\n");
            }
        }
    }

    for (const std::unique_ptr<PlayerBullet>& bullet : player_->GetBullets()) {
        if (!bullet->IsAlive()) {
            continue;
        }

        bool bulletHit = false;

        for (std::unique_ptr<BaseEnemy>& enemy : enemies_) {

            if (enemy->IsDead()) {
                continue;
            }

            enemyCollisionParts.clear();
            enemy->GetCollisionParts(enemyCollisionParts);

            for (const EnemyCollisionPart& part : enemyCollisionParts) {
                Vector3 difference = part.position - bullet->GetPosition();

                float distance = Vector3Length(difference);

                float collisionRadius = part.radius + bullet->GetCollisionRadius();

                if (distance <= collisionRadius) {

                    OutputDebugStringA("PlayerBullet Hit Enemy\n");

                    if (enemy->IsCollisionPartDamageable(part.partIndex)) {
                        bullet->OnHitEnemy(part.position);
                        enemy->ApplyDamageToPart(
                            part.partIndex,
                            static_cast<float>(bullet->GetDamage()));
                    } else {
                        enemy->OnCollisionPartGuarded(
                            part.partIndex,
                            part.position);
                    }

                    bullet->SetDead();
                    bulletHit = true;

                    break;
                }
            }

            if (bulletHit) {
                break;
            }
        }
    }

    // プレイヤーの弾とボスの当たり判定
    if (activeBoss_ && !activeBoss_->IsDead()) {
        // ボスのパーツ一覧をループ外で一度だけ取得し、弾ごとのメモリアロケーションを回避
        enemyCollisionParts.clear();
        activeBoss_->GetCollisionParts(enemyCollisionParts);

        for (const std::unique_ptr<PlayerBullet>& bullet : player_->GetBullets()) {
            if (!bullet->IsAlive()) {
                continue;
            }

            for (const EnemyCollisionPart& part : enemyCollisionParts) {
                // コライダーのワールド座標を計算
                Vector3 difference = part.position - bullet->GetPosition();
                float distance = Vector3Length(difference);

                // 判定半径
                float collisionRadius = part.radius + bullet->GetCollisionRadius();

                if (distance <= collisionRadius) {
                    OutputDebugStringA("PlayerBullet Hit Boss\n");

                    if (activeBoss_->IsCollisionPartDamageable(part.partIndex)) {
                        bullet->OnHitEnemy(part.position);
                        activeBoss_->ApplyDamageToPart(
                            part.partIndex,
                            static_cast<float>(bullet->GetDamage()));
                    } else {
                        activeBoss_->OnCollisionPartGuarded(
                            part.partIndex,
                            part.position);
                    }

                    bullet->SetDead();
                    break;
                }
            }
        }
    }

    // プレイヤーの弾と床の当たり判定
    for (const std::unique_ptr<PlayerBullet>& bullet : player_->GetBullets()) {
        if (!bullet->IsAlive()) {
            continue;
        }

        float floorY = -30.0f;
        if (floorObj_) {
            floorY = floorObj_->GetTranslate().y;
        }

        if (bullet->GetPosition().y <= floorY) {
            Vector3 hitPosition = bullet->GetPosition();
            hitPosition.y = floorY;

            EffectManager::GetInstance()->PlayEffect("HitEffect", hitPosition);
            bullet->SetDead();
        }
    }

    // 死んだ敵の中から「NormalEnemy」だけを選んで安Eに削除する
    std::erase_if(enemies_, [](const std::unique_ptr<BaseEnemy>& enemy) {
        return enemy->IsDead();
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
            float distance = Vector3Length(difference);
            float collisionRadius = enemyBullet->GetCollisionRadius();

            if (distance <= collisionRadius) {
                OutputDebugStringA("EnemyBullet Hit Player\n");

                enemyBullet->OnHitPlayer(player_->GetTranslate());

                if (player_->ApplyDamage(enemyBullet->GetDamage())) {
                    EffectManager::GetInstance()->PlayEffect("DamageHit", player_->GetTranslate());

                    if (player_->IsDead()) {
                        ResetGameplayPostEffects();
                        SceneManager::GetInstance()->SetNextScene(std::make_unique<GameOverScene>());
                    }
                }
                enemyBullet->SetDead();
                // 1発の弾で複数回ダメージを受けないように、当たったらすぐに弾を無効化する
            }
        }
    }

    // ボスの弾とプレイヤーの当たり判定
    if (activeBoss_ && !activeBoss_->IsDead()) {
        for (const std::unique_ptr<EnemyBullet>& enemyBullet : activeBoss_->GetBullets()) {
            if (!enemyBullet->IsAlive()) {
                continue;
            }

            Vector3 difference = enemyBullet->GetPosition() - player_->GetTranslate();
            float distance = Vector3Length(difference);
            float collisionRadius = enemyBullet->GetCollisionRadius();

            if (distance <= collisionRadius) {
                OutputDebugStringA("BossBullet Hit Player\n");

                enemyBullet->OnHitPlayer(player_->GetTranslate());

                if (player_->ApplyDamage(enemyBullet->GetDamage())) {
                    EffectManager::GetInstance()->PlayEffect("DamageHit", player_->GetTranslate());

                    if (player_->IsDead()) {
                        ResetGameplayPostEffects();
                        SceneManager::GetInstance()->SetNextScene(std::make_unique<GameOverScene>());
                    }
                }
                enemyBullet->SetDead();
            }
        }
    }
}

void GamePlayScene::Finalize()
{
    CollisionManager::GetInstance()->SetEnemies(nullptr);
    ResetGameplayPostEffects();

    // シーン内で再生していたエフェクトだけを停止する。
    // シェーダーやパイプラインは次回のゲームシーンで再利用する。
    EffectManager::GetInstance()->StopAllEffects();
    EffectManager::GetInstance()->SetCamera(nullptr);
    playerJetHandle_ = kInvalidEffectHandle;
    playerJetSparkHandle_ = kInvalidEffectHandle;
    boostLineHandle_ = kInvalidEffectHandle;

    // SoundManager::GetInstance()->SoundUnload(&bgm);
}

void GamePlayScene::ResetGameplayPostEffects()
{
    SceneManager::GetInstance()->ClearPostEffects();
    SceneManager::GetInstance()->SetPostEffectCenter({ 0.5f, 0.5f });
    SceneManager::GetInstance()->SetPostEffectKickStrength(0.0f);

    boostKickTimer_ = 0.0f;
    boostKickStrength_ = 0.0f;
    wasBoostingForKick_ = false;
    wasPlayerBoosting_ = false;
    smoothedBoostPostEffectCenter_ = { 0.5f, 0.5f };
}

void GamePlayScene::LoadEnemyPopData()
{
    std::string filePath = "resources/Data/enemyPop.json";
    std::ifstream file(filePath);

    if (file.is_open() == false) {
        Logger::Log("Warning: Could not open " + filePath + ". Using default enemy layout.");
        
        const Vector3 enemyPositions[] = {
            { -14.0f, -3.0f, 384.0f },
            { 10.0f, 5.0f, 512.0f },
            { 18.0f, -6.0f, 672.0f },
            { -7.0f, 7.0f, 832.0f },
            { 14.0f, 0.0f, 992.0f },
            { -18.0f, 4.0f, 1152.0f },
            { 4.0f, -7.0f, 1312.0f },
            { 17.0f, 6.0f, 1472.0f },
            { -12.0f, 1.0f, 1632.0f },
            { 7.0f, -5.0f, 1792.0f },
            { -19.0f, 0.0f, 1440.0f },
        };

        int32_t enemyIndex = 0;
        for (const Vector3& enemyPosition : enemyPositions) {
            if (enemyIndex % 2 == 0) {
                std::unique_ptr<MoveEnemy> enemy = std::make_unique<MoveEnemy>();
                enemy->Initialize(enemyModel_, enemyBulletModel_, player_.get());
                enemy->SetPosition(enemyPosition);

                if (enemyIndex % 8 == 0) {
                    enemy->SetMovePattern(MovePattern::LeftRight);
                    enemy->SetAmplitude(8.0f);
                    enemy->SetMoveSpeed(2.0f);
                }
                if (enemyIndex % 8 == 2) {
                    enemy->SetMovePattern(MovePattern::UpDown);
                    enemy->SetAmplitude(6.0f);
                    enemy->SetMoveSpeed(2.0f);
                }
                if (enemyIndex % 8 == 4) {
                    enemy->SetMovePattern(MovePattern::ZigZag);
                    enemy->SetAmplitude(8.0f);
                    enemy->SetMoveSpeed(2.5f);
                }
                if (enemyIndex % 8 == 6) {
                    enemy->SetMovePattern(MovePattern::SineWave);
                    enemy->SetAmplitude(5.0f);
                    enemy->SetFrequency(3.0f);
                    enemy->SetMoveSpeed(1.5f);
                }

                enemies_.push_back(std::move(enemy));
            } else {
                std::unique_ptr<NormalEnemy> enemy = std::make_unique<NormalEnemy>();
                enemy->Initialize(enemyModel_, enemyBulletModel_, player_.get());
                enemy->SetPosition(enemyPosition);
                enemies_.push_back(std::move(enemy));
            }
            enemyIndex = enemyIndex + 1;
        }

        return;
    }

    nlohmann::json root;
    file >> root;
    file.close();

    if (root.contains("enemies") && root["enemies"].is_array()) {
        nlohmann::json enemiesArray = root["enemies"];

        for (size_t i = 0; i < enemiesArray.size(); i = i + 1) {
            nlohmann::json enemyData = enemiesArray[i];

            std::string type = enemyData["type"].get<std::string>();
            std::vector<float> positionArray = enemyData["position"].get<std::vector<float>>();
            Vector3 position = { positionArray[0], positionArray[1], positionArray[2] };

            if (type == "NormalEnemy") {
                std::unique_ptr<NormalEnemy> enemy = std::make_unique<NormalEnemy>();
                enemy->Initialize(enemyModel_, enemyBulletModel_, player_.get());
                enemy->SetPosition(position);
                enemies_.push_back(std::move(enemy));
            }

            if (type == "MoveEnemy") {
                std::unique_ptr<MoveEnemy> enemy = std::make_unique<MoveEnemy>();
                enemy->Initialize(enemyModel_, enemyBulletModel_, player_.get());
                enemy->SetPosition(position);

                if (enemyData.contains("movePattern")) {
                    std::string patternStr = enemyData["movePattern"].get<std::string>();
                    if (patternStr == "LeftRight") {
                        enemy->SetMovePattern(MovePattern::LeftRight);
                    }
                    if (patternStr == "UpDown") {
                        enemy->SetMovePattern(MovePattern::UpDown);
                    }
                    if (patternStr == "ZigZag") {
                        enemy->SetMovePattern(MovePattern::ZigZag);
                    }
                    if (patternStr == "SineWave") {
                        enemy->SetMovePattern(MovePattern::SineWave);
                    }
                }

                if (enemyData.contains("moveSpeed")) {
                    enemy->SetMoveSpeed(enemyData["moveSpeed"].get<float>());
                }
                if (enemyData.contains("amplitude")) {
                    enemy->SetAmplitude(enemyData["amplitude"].get<float>());
                }
                if (enemyData.contains("frequency")) {
                    enemy->SetFrequency(enemyData["frequency"].get<float>());
                }

                enemies_.push_back(std::move(enemy));
            }

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
        boostKickTimer_ -= kBoostKickFrameTime;
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
        smoothedBoostPostEffectCenter_ = LerpVector2(smoothedBoostPostEffectCenter_, targetCenter, kBoostPostEffectCenterLerpRate);
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

    center.x = ClampFloat(center.x, kBoostPostEffectCenterMin, kBoostPostEffectCenterMax);
    center.y = ClampFloat(center.y, kBoostPostEffectCenterMin, kBoostPostEffectCenterMax);

    return center;
}

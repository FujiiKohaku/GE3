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
    camera_ = std::make_unique<Camera>();
    camera_->Initialize();
    camera_->SetTranslate({ 0.0f, 3.0f, -30.0f });
    camera_->SetRotate({ 0.0f, 0.0f, 0.0f });
    normalFovY_ = camera_->GetFovY();
    currentFovY_ = normalFovY_;
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
    ModelManager::GetInstance()->Load("dolone.obj");
    ModelManager::GetInstance()->Load("sneakWalk.gltf");
    ModelManager::GetInstance()->Load("AnimatedCube.gltf");
    Model* playerModel = ModelManager::GetInstance()->Load("AirPlane.obj");

    Model* enemyModel_ = ModelManager::GetInstance()->Load("star.obj");
    Model* enemyBulletModel_ = ModelManager::GetInstance()->Load("star.obj");
    // animationskinLoad
    // skinningWalk
    ModelManager::GetInstance()->Load("walk.gltf");
    //==============
    //  OBJ
    //==============
    Object3d* terrain_ = sceneObjectManager_->CreateObject("terrain", "terrain.obj");

    Object3d* star = sceneObjectManager_->CreateObject("star", "star.obj");

    animationActor_ = std::make_unique<AnimationActor>();
    OutputDebugStringA("A\n");
    animationActor_->Initialize("sneakWalk.gltf");
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

    aimSprite_ = std::make_unique<Sprite>();
    aimSprite_->Initialize(SpriteManager::GetInstance(), "resources/Textures/aim.png");
    aimSprite_->SetSize({ 128.0f, 128.0f });
    aimSprite_->SetAnchorPoint({ 0.5f, 0.5f });
    aimSprite_->SetPosition({ WinApp::GetInstance()->kClientWidth / 2.0f, WinApp::GetInstance()->kClientHeight / 2.0f });
    aimSprite_->Update();
    TextureManager::GetInstance()->LoadTexture("resources/Textures/uvChecker.png");
    skyBox_ = std::make_unique<SkyBox>();
    skyBox_->Initialize(DirectXCommon::GetInstance());
    skyBox_->SetTexture("resources/Textures/skybox.dds");

    // =================================================
    // Playerクラス
    // =================================================
    player_ = std::make_unique<Player>();
    player_->Initialize(playerModel);
    player_->SetCamera(camera_.get());
    player_->SetDebugCameraController(debugCameraController_.get());
    player_->SetTranslate({ 0.0f, 0.0f, 0.0f });
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
    rail_->Update();

    for (std::unique_ptr<BaseEnemy>& enemy : enemies_) {
        enemy->Update();
    }

    // プレイヤ�E��EZ座標�E取征E
    float playerZ = player_->GetTranslate().z;

    // エチE��ターマネージャーの更新�E�カメラを渡す！E
    editorManager_->Update(camera_.get());

    // for (std::unique_ptr<Object3d>& levelObject : levelObjects_) {
    //     levelObject->Update();
    // }
    // if (Input::GetInstance()->IsKeyTrigger(DIK_1)) {
    //    SceneManager::GetInstance()->SetPostEffectType(PostEffectType::Copy);
    //}

    // if (Input::GetInstance()->IsKeyTrigger(DIK_2)) {
    //     SceneManager::GetInstance()->SetPostEffectType(PostEffectType::GrayScale);
    // }
    // if (Input::GetInstance()->IsKeyTrigger(DIK_3)) {
    //     SceneManager::GetInstance()->SetPostEffectType(PostEffectType::Vignette);
    // }
    // if (Input::GetInstance()->IsKeyTrigger(DIK_4)) {
    //     SceneManager::GetInstance()->SetPostEffectType(PostEffectType::smoothing);
    // }
    // if (Input::GetInstance()->IsKeyTrigger(DIK_5)) {
    //     SceneManager::GetInstance()->SetPostEffectType(PostEffectType::GaussianFilter);
    // }
    // if (Input::GetInstance()->IsKeyTrigger(DIK_6)) {
    //     SceneManager::GetInstance()->SetPostEffectType(PostEffectType::LuminanceBasedOutline);
    // }
    // if (Input::GetInstance()->IsKeyTrigger(DIK_7)) {
    //     SceneManager::GetInstance()->SetPostEffectType(PostEffectType::DepthOutline);
    // }

    // if (Input::GetInstance()->IsKeyTrigger(DIK_8)) {
    //     SceneManager::GetInstance()->SetPostEffectType(PostEffectType::RadialBlur);
    // }
    // if (Input::GetInstance()->IsKeyTrigger(DIK_9)) {
    //     SceneManager::GetInstance()->SetPostEffectType(PostEffectType::Dissolve);
    // }
    // if (Input::GetInstance()->IsKeyTrigger(DIK_F10)) {
    //     SceneManager::GetInstance()->SetNextScene(std::make_unique<GameOverScene>());
    // }
    // if (Input::GetInstance()->IsKeyTrigger(DIK_F11)) {
    //     SceneManager::GetInstance()->SetNextScene(std::make_unique<ClearScene>());
    // }
    float nextRailDistance = railDistance_ + railSpeed_;
    if (nextRailDistance > rail_->GetTotalLength()) {
        nextRailDistance = rail_->GetTotalLength();
    }

    Vector3 currentPosition = rail_->GetPositionByDistance(nextRailDistance);
    Vector3 forward = CalculateRailForward(nextRailDistance, currentPosition);

    Vector3 railRight {};
    Vector3 railUp {};
    CalculateRailBasis(forward, railRight, railUp);

    bool isBoostingForCamera = false;
    Input* input = Input::GetInstance();
    if (input != nullptr) {
        isBoostingForCamera = input->IsKeyPressed(DIK_LSHIFT);

        if (input->IsKeyTrigger(DIK_L)) {
            isRandomPostEffect_ = !isRandomPostEffect_;
            hasRandomPostEffectToggle_ = true;
        }
    }

    float targetFovY = normalFovY_;
    if (isBoostingForCamera) {
        targetFovY = boostFovY_;
    }

    currentFovY_ += (targetFovY - currentFovY_) * fovLerpRate_;
    camera_->SetFovY(currentFovY_);

    debugCameraController_->Update();

    if (!debugCameraController_->GetDebugMode()) {
        Vector3 cameraForward = forward;

        if (hasCameraFollowState_) {
            Vector3 lerpedForward = Lerp(smoothedCameraForward_, forward, cameraForwardLerpRate_);

            if (!IsZeroVector(lerpedForward)) {
                cameraForward = Normalize(lerpedForward);
            }
        }

        smoothedCameraForward_ = cameraForward;

        Vector3 targetCameraPosition = currentPosition - cameraForward * 35.0f;
        targetCameraPosition.y += 6.0f;

        Vector3 targetLookAheadPosition = rail_->GetPositionByDistance(nextRailDistance + cameraLookAheadDistance_);

        if (hasCameraFollowState_) {
            smoothedCameraPosition_ = Lerp(smoothedCameraPosition_, targetCameraPosition, cameraFollowLerpRate_);
            smoothedLookAheadPosition_ = Lerp(smoothedLookAheadPosition_, targetLookAheadPosition, cameraFollowLerpRate_);
        } else {
            smoothedCameraPosition_ = targetCameraPosition;
            smoothedLookAheadPosition_ = targetLookAheadPosition;
            hasCameraFollowState_ = true;
        }

        camera_->LookAt(smoothedCameraPosition_, smoothedLookAheadPosition_);
    } else {
        hasCameraFollowState_ = false;
    }

    camera_->Update();

    player_->SetRailFrame(currentPosition, railRight, railUp);

    //  プレイヤーの更新�E��E力�E琁E��移動など�E�E
    player_->Update();

    railDistance_ = nextRailDistance;

    if (forward.x != 0.0f || forward.y != 0.0f || forward.z != 0.0f) {
        float horizontalLength = std::sqrt(forward.x * forward.x + forward.z * forward.z);

        Vector3 playerRotate {};
        playerRotate.x = -std::atan2(forward.y, horizontalLength);
        playerRotate.y = -std::atan2(forward.x, forward.z);
        playerRotate.z = 0.0f;

        player_->SetRotate(playerRotate);
    }

    Vector3 railOffset = player_->GetRailOffset();
    Vector3 playerPosition = currentPosition;
    playerPosition += railRight * railOffset.x;
    playerPosition += railUp * railOffset.y;
    player_->SetTranslate(playerPosition);

    DebugRenderer::GetInstance()->AddLine(
        currentPosition,
        currentPosition + forward * 20.0f,
        { 0.0f, 1.0f, 0.0f, 1.0f },
        3.0f);

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

    // AiMスプライチEの位置を更新する
    aimSprite_->SetPosition(player_->GetAimScreenPosition());
    aimSprite_->Update();

    skyBox_->Update(camera_.get());

    // パーティクルを発生させる

    EffectManager::GetInstance()->Update();

    sceneObjectManager_->Update();

    // アニメーションアクターの更新
    animationActor_->Update(1.0f / 60.0f);
    CheckCollision();
#pragma region
#ifdef USE_IMGUI

    // player_->DrawImGui();

    // ==================================
    // Lighting Panel�E�ライト操作パネル�E�E
    // ==================================
    ImGui::Begin("Lighting Control");

    // ---- ライト�E ON / OFF ----
    static bool lightEnabled = false;
    ImGui::Checkbox("Enable Light", &lightEnabled);

    // ---- ライト�E色 ----
    static Vector4 lightColor = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    ImGui::ColorEdit3("Light Color", (float*)&lightColor);

    // ---- 明るさ（強さ！E----
    static float lightIntensity = 1.0f;
    ImGui::SliderFloat("Intensity", &lightIntensity, 0.0f, 5.0f);

    // ---- 光�E向き ----
    static Vector3 lightDir = { 0.0f, -1.0f, 0.0f };
    ImGui::SliderFloat3("Direction", &lightDir.x, -1.0f, 1.0f);

    // ---- 正規化 ----
    Vector3 normalizedDir = Normalize(lightDir);

    float intensity = lightIntensity;
    if (!lightEnabled) {
        intensity = 0.0f; // OFF のとき�E光なぁE
    }

    LightManager::GetInstance()->SetDirectional(
        { lightColor.x, lightColor.y, lightColor.z, 1.0f },
        normalizedDir,
        intensity);

    // ---- リセチE��ボタン�E�向きだけ�Eに戻す！E---
    if (ImGui::Button("Reset Direction")) {
        lightDir = { 0.0f, -1.0f, 0.0f };
    }

    ImGui::SameLine();

    // ---- ライトを完�E初期匁E----
    if (ImGui::Button("Reset Light")) {
        lightEnabled = true;
        lightColor = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
        lightIntensity = 1.0f;
        lightDir = { 0.0f, -1.0f, 0.0f };
    }
    ImGui::Separator();
    ImGui::Text("Point Light Control");

    static bool pointEnabled = false;
    ImGui::Checkbox("Enable Point Light", &pointEnabled);

    static Vector4 pointColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    ImGui::ColorEdit3("Point Color", (float*)&pointColor);

    static Vector3 pointPos = { 0.0f, 2.0f, 0.0f };
    ImGui::SliderFloat3("Point Position", &pointPos.x, -10.0f, 10.0f);

    static float pointIntensity = 1.0f;
    ImGui::SliderFloat("Point Intensity", &pointIntensity, 0.0f, 5.0f);

    float pI = pointEnabled ? pointIntensity : 0.0f;
    static float pointRadius = 10.0f;
    static float pointDecay = 1.0f;

    ImGui::SliderFloat("Point Radius", &pointRadius, 0.1f, 30.0f);
    ImGui::SliderFloat("Point Decay", &pointDecay, 0.1f, 5.0f);

    LightManager::GetInstance()->SetPointRadius(pointRadius);
    LightManager::GetInstance()->SetPointDecay(pointDecay);
    LightManager::GetInstance()->SetPointLight(pointColor, pointPos, pI);
    ImGui::Separator();
    ImGui::Text("Spot Light Control");
    // ================================
    // Spot Light Control
    // ================================

    static bool spotEnabled = true;
    ImGui::Checkbox("Enable Spot Light", &spotEnabled);

    // 色
    static Vector4 spotColor = { 1, 1, 1, 1 };
    ImGui::ColorEdit3("Spot Color", (float*)&spotColor);

    // 位置
    static Vector3 spotPos = { 0.0f, 0.0f, 0.0f };
    ImGui::SliderFloat3("Spot Position", &spotPos.x, -10.0f, 10.0f);

    // 方吁E
    static Vector3 spotDir = { -1.0f, 0.0f, 0.0f };
    ImGui::SliderFloat3("Spot Direction", &spotDir.x, -1.0f, 1.0f);
    Vector3 normalizedSpotDir = Normalize(spotDir);

    // 強ぁE
    static float spotIntensity = 4.0f;
    ImGui::SliderFloat("Spot Intensity", &spotIntensity, 0.0f, 10.0f);

    // 距離・減衰
    static float spotDistance = 7.0f;
    static float spotDecay = 2.0f;
    ImGui::SliderFloat("Spot Distance", &spotDistance, 0.1f, 30.0f);
    ImGui::SliderFloat("Spot Decay", &spotDecay, 0.1f, 5.0f);

    // 角度�E�度数で操佁EↁEcos に変換�E�E
    static float spotAngleDeg = 60.0f;
    static float spotFalloffStartDeg = 30.0f;

    ImGui::SliderFloat("Spot Angle (deg)", &spotAngleDeg, 1.0f, 90.0f);
    ImGui::SliderFloat("Falloff Start (deg)", &spotFalloffStartDeg, 1.0f, spotAngleDeg - 1.0f);

    // cos に変換
    float cosAngle = std::cos(spotAngleDeg * std::numbers::pi_v<float> / 180.0f);
    float cosFalloffStart = std::cos(spotFalloffStartDeg * std::numbers::pi_v<float> / 180.0f);

    // OFF のとぁE
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

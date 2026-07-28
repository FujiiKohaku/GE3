#include "TestScene1.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/3D/SkinningObject3dManager.h"
#include "Engine/Light/LightManager.h"
#include "Engine/input/Input.h"
#include "TitleScene.h"
#include "externals/imgui/imgui.h"
#include "Engine/3D/ModelManager.h"
#include "Engine/DirectXCommon/DirectXCommon.h"
#include "Engine/Animation/AnimationLoder.h"
#include "Engine/TextureManager/TextureManager.h"
#include "Engine/Effect/EffectManager.h"
#include <numbers>
#include <algorithm>
#include <cmath>
#include <random>

void TestScene1::Initialize()
{
    // カメラの初期化
    camera_ = std::make_unique<Camera>();
    camera_->Initialize();
    camera_->LookAt({ 0.0f, 15.0f, -35.0f }, { 0.0f, 0.0f, 0.0f });
    camera_->Update();
    Object3dManager::GetInstance()->SetDefaultCamera(camera_.get());
    SkinningObject3dManager::GetInstance()->SetDefaultCamera(camera_.get());

    debugCameraController_ = std::make_unique<DebugCameraController>();
    debugCameraController_->SetTargetCamera(camera_.get());
    debugCameraController_->SetArrowKeyRotationEnabled(false);
    debugCameraController_->SetRotationMouseButton(1);
    debugCameraController_->SetDebugMode(false);

    // 環境マップの設定
    TextureManager::GetInstance()->LoadTexture("resources/Textures/skybox.dds");
    D3D12_GPU_DESCRIPTOR_HANDLE skyboxHandle = TextureManager::GetInstance()->GetSrvHandleGPU("resources/Textures/skybox.dds");
    Object3dManager::GetInstance()->SetEnvironmentTexture(skyboxHandle);
    SkinningObject3dManager::GetInstance()->SetEnvironmentTexture(skyboxHandle);

    LightManager* lightManager = LightManager::GetInstance();
    lightManager->SetDirectional(
        { 1.0f, 0.52f, 0.25f, 1.0f },
        { 0.45f, -1.0f, 0.25f },
        0.48f);
    lightManager->SetAmbientColor({ 0.16f, 0.20f, 0.38f });
    lightManager->SetAmbientIntensity(0.14f);
    lightManager->SetPointColor({ 1.0f, 0.38f, 0.16f, 1.0f });
    lightManager->SetPointIntensity(0.22f);
    lightManager->SetPointRadius(12.0f);
    lightManager->SetSpotLightColor({ 0.20f, 0.28f, 0.58f, 1.0f });
    lightManager->SetSpotLightIntensity(0.55f);
    lightManager->SetSpotLightDistance(10.0f);

    // floorの初期化
    Model* floorModel = ModelManager::GetInstance()->CreatePlane("resources/Textures/floor_dirt_gemini.jpg", 10.0f, 10.0f);
    floorObj_ = std::make_unique<Object3d>();
    floorObj_->Initialize(Object3dManager::GetInstance());
    floorObj_->SetEnableLighting(true);
    floorObj_->SetModel(floorModel);
    floorObj_->SetTranslate({ 0.0f, -5.0f, 0.0f });
    floorObj_->SetRotate({ std::numbers::pi_v<float> / 2.0f, 0.0f, 0.0f });
    floorObj_->SetScale({ 100.0f, 100.0f, 1.0f });

    // Robo Playerの初期化
    const std::string playerModelPath = "Characters/precision_robot_rigged_single_gltf/precision_robot_rigged_single.gltf";
    playerActor_ = std::make_unique<AnimationActor>();
    playerActor_->Initialize(playerModelPath);
    playerActor_->GetObject()->SetEnableLighting(true);

    // アニメーションのロード
    idleAnimation_   = AnimationLoder::LoadAnimationFile("resources/Models", playerModelPath, 4); // Robot_Idle
    combatIdleAnimation_ = AnimationLoder::LoadAnimationFile("resources/Models", playerModelPath, 1); // Robot_CombatIdle
    attackAnimation_ = AnimationLoder::LoadAnimationFile("resources/Models", playerModelPath, 6); // Robot_Punch
    leftPunchAnimation_ = AnimationLoder::LoadAnimationFile("resources/Models", playerModelPath, 7); // Robot_Punch_L
    rocketUppercutAnimation_ = AnimationLoder::LoadAnimationFile("resources/Models", playerModelPath, 8); // Robot_RocketUppercut
    jumpAnimation_   = AnimationLoder::LoadAnimationFile("resources/Models", playerModelPath, 5); // Robot_Jump
    runAnimation_    = AnimationLoder::LoadAnimationFile("resources/Models", playerModelPath, 9); // Robot_Walk
    dashAnimation_   = AnimationLoder::LoadAnimationFile("resources/Models", playerModelPath, 2); // Robot_Dash

    playerPos_ = { 0.0f, -5.0f, 0.0f };
    playerRot_ = { 0.0f, 0.0f, 0.0f };
    playerActor_->SetTranslate(playerPos_);
    playerActor_->SetRotate(playerRot_);
    playerActor_->SetScale({ playerScale_, playerScale_, playerScale_ });

    Model* katanaModel = ModelManager::GetInstance()->Load("Characters/cyan_katana/cyan_katana.obj");
    katanaObj_ = std::make_unique<Object3d>();
    katanaObj_->Initialize(Object3dManager::GetInstance());
    katanaObj_->SetEnableLighting(true);
    katanaObj_->SetModel(katanaModel);

    Model* recoveryCubeModel =
        ModelManager::GetInstance()->Load(
            "Debug/Samples/AnimatedCube/AnimatedCube.gltf");
    recoveryCubeObj_ = std::make_unique<Object3d>();
    recoveryCubeObj_->Initialize(Object3dManager::GetInstance());
    recoveryCubeObj_->SetModel(recoveryCubeModel);
    recoveryCubeObj_->SetTranslate(recoveryCubeBasePosition_);
    recoveryCubeObj_->SetScale({ 0.75f, 0.75f, 0.75f });
    recoveryCubeObj_->SetColor({ 0.20f, 1.0f, 0.35f, 1.0f });
    recoveryCubeObj_->SetEnableLighting(true);
    recoveryCubeObj_->Update();

    // Fixed SneakWalk model for skeleton debug display
    sneakWalkActor_ = std::make_unique<AnimationActor>();
    sneakWalkActor_->Initialize("Characters/Animation/SneakWalk/sneakWalk.gltf");
    sneakWalkActor_->GetObject()->SetEnableLighting(true);
    sneakWalkActor_->SetTranslate({ 7.0f, -5.0f, 0.0f });
    sneakWalkActor_->SetRotate({ 0.0f, std::numbers::pi_v<float>, 0.0f });
    sneakWalkActor_->SetScale({ 2.0f, 2.0f, 2.0f });
    sneakWalkActor_->SetSkeletonDebugVisible(true);

    // 初期アニメーションの設定
    playerActor_->GetPlayAnimation()->SetAnimation(&idleAnimation_);
    currentAnimState_ = PlayerAnimState::Idle;

    // エフェクトマネージャーへのカメラ登録
    EffectManager::GetInstance()->SetCamera(camera_.get());

    // Local WindとLocal Vortexを設定した明るい粒子をフィールド確認用に常設する
    fieldDemoEffectHandle_ = EffectManager::GetInstance()->PlayLoopEffect(
        "FieldDemo",
        { -7.0f, -4.5f, 0.0f });
    cyberSingularityEffectHandle_ = EffectManager::GetInstance()->PlayLoopEffect(
        "CyberSingularity",
        { 0.0f, -2.0f, 8.0f });
    recoveryEffectHandle_ =
        EffectManager::GetInstance()->PlayLoopEffect(
            "HealPickup",
            recoveryCubeBasePosition_);
    const Vector3 groundGlyphPosition = {
        playerPos_.x, playerPos_.y + 0.06f, playerPos_.z
    };
    groundLightningOuterHandle_ =
        EffectManager::GetInstance()->PlayLoopEffect(
            "GroundLightningOuter", groundGlyphPosition);
    groundLightningInnerHandle_ =
        EffectManager::GetInstance()->PlayLoopEffect(
            "GroundLightningInner", groundGlyphPosition);
    groundLightningMotesHandle_ =
        EffectManager::GetInstance()->PlayLoopEffect(
            "GroundLightningMotes", groundGlyphPosition);

    snowEffectHandle_ = EffectManager::GetInstance()->PlayLoopEffect("Snow", playerPos_);

    leftHandFlameHandle_ = EffectManager::GetInstance()->PlayLoopEffect("HandFlame", playerPos_);
    rightHandFlameHandle_ = EffectManager::GetInstance()->PlayLoopEffect("HandFlame", playerPos_);

    selectedPostEffectIndex_ = 0;
    ApplySelectedPostEffect();
}

void TestScene1::Update()
{
    // タイトルシーンに戻る
    if (Input::GetInstance()->IsKeyTrigger(DIK_ESCAPE)) {
        SceneManager::GetInstance()->SetNextScene(std::make_unique<TitleScene>());
        return;
    }

    LONG mouseWheel = Input::GetInstance()->GetMouseWheel();
    bool canSwitchPostEffect = true;
#ifdef USE_IMGUI
    canSwitchPostEffect = !ImGui::GetIO().WantCaptureMouse;
#endif
    if (canSwitchPostEffect && mouseWheel > 0) {
        if (selectedPostEffectIndex_ == 0) {
            selectedPostEffectIndex_ = postEffectTypes_.size() - 1;
        } else {
            selectedPostEffectIndex_--;
        }
        ApplySelectedPostEffect();
    } else if (canSwitchPostEffect && mouseWheel < 0) {
        selectedPostEffectIndex_++;
        if (selectedPostEffectIndex_ >= postEffectTypes_.size()) {
            selectedPostEffectIndex_ = 0;
        }
        ApplySelectedPostEffect();
    }

    // 1. マウスによるTPSカメラ操作（回転）は無効化 (完全固定カメラ)
    cameraYaw_ = 0.0f;
    cameraPitch_ = 0.35f;

    // 2. WASDによる移動操作 (攻撃中は動けない)
    Vector3 moveDir = { 0.0f, 0.0f, 0.0f };
    bool hasMoveInput = false;

    if (currentAnimState_ != PlayerAnimState::Attacking) {
        if (Input::GetInstance()->IsKeyPressed(DIK_UP)) {
            moveDir.z += 1.0f;
        }
        if (Input::GetInstance()->IsKeyPressed(DIK_DOWN)) {
            moveDir.z -= 1.0f;
        }
        if (Input::GetInstance()->IsKeyPressed(DIK_LEFT)) {
            moveDir.x -= 1.0f;
        }
        if (Input::GetInstance()->IsKeyPressed(DIK_RIGHT)) {
            moveDir.x += 1.0f;
        }

        moveDir.x += Input::GetInstance()->GetGamepadLeftStickX();
        moveDir.z += Input::GetInstance()->GetGamepadLeftStickY();

        hasMoveInput = (moveDir.x != 0.0f || moveDir.z != 0.0f);
    }

    bool isDashInput = false;
    if (hasMoveInput) {
        bool isLeftShiftPressed = Input::GetInstance()->IsKeyPressed(DIK_LSHIFT);
        bool isRightShiftPressed = Input::GetInstance()->IsKeyPressed(DIK_RSHIFT);
        bool isGamepadDashPressed =
            Input::GetInstance()->IsGamepadButtonPressed(XINPUT_GAMEPAD_B);
        isDashInput =
            isLeftShiftPressed ||
            isRightShiftPressed ||
            isGamepadDashPressed;
    }

    if (hasMoveInput) {
        moveDir = Normalize(moveDir);

        Vector3 worldMoveDir = NormalizeSafe(moveDir);

        float speed = 0.15f;
        if (isDashInput) {
            speed = 0.4f;
        }
        playerPos_.x += worldMoveDir.x * speed;
        playerPos_.z += worldMoveDir.z * speed;

        // プレイヤーの向きを入力方向に合わせる (滑らかな旋回補間) (X軸の旋回方向を反転させてA/Dの向きを修正)
        float targetYaw = std::atan2(-worldMoveDir.x, worldMoveDir.z) + playerRotOffset_;
        float currentYaw = playerRot_.y;

        float diff = targetYaw - currentYaw;
        while (diff < -std::numbers::pi_v<float>) diff += 2.0f * std::numbers::pi_v<float>;
        while (diff > std::numbers::pi_v<float>) diff -= 2.0f * std::numbers::pi_v<float>;

        playerRot_.y = currentYaw + diff * 0.15f; // 旋回補正
    } else {
        // 移動していないときは向きを変更せず、直前の向きをそのままキープする
    }

    // 3. ジャンプ、攻撃、およびアニメーションステート制御
    if (currentAnimState_ == PlayerAnimState::Attacking) {
        attackTimer_ += 1.0f / 60.0f;

        if (Input::GetInstance()->IsMouseTrigger(0) ||
            Input::GetInstance()->IsGamepadButtonTrigger(XINPUT_GAMEPAD_X)) {
            if (comboStep_ + queuedComboAttacks_ < 2) {
                queuedComboAttacks_++;
            }
        }

        // 攻撃アニメーション終了判定
        float attackStepDuration = 0.7f;
        if (comboStep_ == 2) {
            attackStepDuration = 1.1f;
        }

        if (attackTimer_ >= attackStepDuration) {
            if (queuedComboAttacks_ > 0 && comboStep_ < 2) {
                comboStep_++;
                queuedComboAttacks_--;
                attackTimer_ = 0.0f;

                if (comboStep_ == 1) {
                    playerActor_->GetPlayAnimation()->SetAnimation(&leftPunchAnimation_, 0.1f);
                } else {
                    playerActor_->GetPlayAnimation()->SetAnimation(&rocketUppercutAnimation_, 0.1f);
                }
            } else {
                currentAnimState_ = PlayerAnimState::Idle;
                playerActor_->GetPlayAnimation()->SetAnimation(&idleAnimation_, 0.2f);
                comboStep_ = 0;
                queuedComboAttacks_ = 0;
            }
        }
    }
    else if (!isJumping_) {
        // 地上にいる場合
        if (Input::GetInstance()->IsMouseTrigger(0) ||
            Input::GetInstance()->IsGamepadButtonTrigger(XINPUT_GAMEPAD_X)) {
            // 攻撃開始 (両手ビームアニメーション)
            currentAnimState_ = PlayerAnimState::Attacking;
            attackTimer_ = 0.0f;
            comboStep_ = 0;
            queuedComboAttacks_ = 0;
            idleVariationTimer_ = 0.0f;
            combatIdleTimer_ = 0.0f;
            playerActor_->GetPlayAnimation()->SetAnimation(&attackAnimation_, 0.1f);
        }
        else if (Input::GetInstance()->IsKeyTrigger(DIK_SPACE) ||
                 Input::GetInstance()->IsGamepadButtonTrigger(XINPUT_GAMEPAD_A)) {
            // ジャンプ開始
            isJumping_ = true;
            jumpVelocity_ = 0.35f;
            idleVariationTimer_ = 0.0f;
            combatIdleTimer_ = 0.0f;
            currentAnimState_ = PlayerAnimState::Jumping;
            playerActor_->GetPlayAnimation()->SetAnimation(&jumpAnimation_, 0.2f);
        } else {
            // 歩行/待機のステート変更
            if (hasMoveInput) {
                idleVariationTimer_ = 0.0f;
                combatIdleTimer_ = 0.0f;
                if (isDashInput) {
                    if (currentAnimState_ != PlayerAnimState::Dashing) {
                        currentAnimState_ = PlayerAnimState::Dashing;
                        playerActor_->GetPlayAnimation()->SetAnimation(&dashAnimation_, 0.15f);
                        PlayDashStartBurst();
                    }
                } else {
                    if (currentAnimState_ != PlayerAnimState::Running) {
                        currentAnimState_ = PlayerAnimState::Running;
                        playerActor_->GetPlayAnimation()->SetAnimation(&runAnimation_, 0.15f);
                    }
                }
            } else {
                if (currentAnimState_ == PlayerAnimState::CombatIdle) {
                    combatIdleTimer_ += 1.0f / 60.0f;
                    if (combatIdleTimer_ >= combatIdleAnimation_.duration) {
                        currentAnimState_ = PlayerAnimState::Idle;
                        playerActor_->GetPlayAnimation()->SetAnimation(&idleAnimation_, 0.2f);
                        idleVariationTimer_ = 0.0f;
                        combatIdleTimer_ = 0.0f;
                    }
                } else if (currentAnimState_ == PlayerAnimState::Idle) {
                    idleVariationTimer_ += 1.0f / 60.0f;
                    if (idleVariationTimer_ >= 6.0f) {
                        currentAnimState_ = PlayerAnimState::CombatIdle;
                        playerActor_->GetPlayAnimation()->SetAnimation(&combatIdleAnimation_, 0.2f);
                        idleVariationTimer_ = 0.0f;
                        combatIdleTimer_ = 0.0f;
                    }
                } else {
                    currentAnimState_ = PlayerAnimState::Idle;
                    playerActor_->GetPlayAnimation()->SetAnimation(&idleAnimation_, 0.2f);
                    idleVariationTimer_ = 0.0f;
                    combatIdleTimer_ = 0.0f;
                }
            }
        }
    } else {
        // 空中にいる場合
        playerPos_.y += jumpVelocity_;
        jumpVelocity_ -= gravity_;

        // 着地判定 (床の高さは -5.0f)
        if (playerPos_.y <= -5.0f) {
            playerPos_.y = -5.0f;
            isJumping_ = false;

            EffectManager::GetInstance()->PlayEffect(
                "LandingDust",
                { playerPos_.x, -4.85f, playerPos_.z });

            // 着地後のステート変更
            if (hasMoveInput) {
                if (isDashInput) {
                    currentAnimState_ = PlayerAnimState::Dashing;
                    playerActor_->GetPlayAnimation()->SetAnimation(&dashAnimation_, 0.2f);
                    PlayDashStartBurst();
                } else {
                    currentAnimState_ = PlayerAnimState::Running;
                    playerActor_->GetPlayAnimation()->SetAnimation(&runAnimation_, 0.2f);
                }
            } else {
                currentAnimState_ = PlayerAnimState::Idle;
                playerActor_->GetPlayAnimation()->SetAnimation(&idleAnimation_, 0.2f);
            }
        }
    }

    // プレイヤーのTransform設定
    playerActor_->SetTranslate(playerPos_);
    playerActor_->SetRotate(playerRot_);
    playerActor_->SetScale({ playerScale_, playerScale_, playerScale_ });
    const Vector3 groundGlyphPosition = {
        playerPos_.x, -4.94f, playerPos_.z
    };
    EffectManager* groundEffectManager = EffectManager::GetInstance();
    if (groundLightningOuterHandle_ != kInvalidEffectHandle &&
        !groundEffectManager->SetEffectPosition(
            groundLightningOuterHandle_, groundGlyphPosition)) {
        groundLightningOuterHandle_ = kInvalidEffectHandle;
    }
    if (groundLightningInnerHandle_ != kInvalidEffectHandle &&
        !groundEffectManager->SetEffectPosition(
            groundLightningInnerHandle_, groundGlyphPosition)) {
        groundLightningInnerHandle_ = kInvalidEffectHandle;
    }
    if (groundLightningMotesHandle_ != kInvalidEffectHandle &&
        !groundEffectManager->SetEffectPosition(
            groundLightningMotesHandle_, groundGlyphPosition)) {
        groundLightningMotesHandle_ = kInvalidEffectHandle;
    }

    debugCameraController_->Update();
    if (!debugCameraController_->GetDebugMode()) {
        Vector3 cameraPos = camera_->GetTranslate();
        cameraPos.x = playerPos_.x;
        camera_->SetTranslate(cameraPos);
    }
    camera_->Update();

    // 更新処理 (止まっている時はアニメーション時間を進めない。ただしブレンド更新中は進める)
    bool isBlending = playerActor_ && playerActor_->GetPlayAnimation() && playerActor_->GetPlayAnimation()->IsBlending();
    float animDeltaTime = 1.0f / 60.0f;
    if (currentAnimState_ == PlayerAnimState::Idle && !isBlending) {
        animDeltaTime = 0.0f;
    }
    playerActor_->Update(animDeltaTime);
    ProcessAnimationEvents();
    UpdateMovementEffects();
    UpdateKatanaAttachment();
    if (sneakWalkActor_) {
        sneakWalkActor_->Update(1.0f / 60.0f);
    }
    if (floorObj_) {
        floorObj_->Update();
    }
    if (recoveryCubeObj_) {
        recoveryCubeAnimationTime_ += 0.035f;
        Vector3 recoveryPosition = recoveryCubeBasePosition_;
        recoveryPosition.y +=
            std::sin(recoveryCubeAnimationTime_) * 0.35f;

        Vector3 recoveryRotation =
            recoveryCubeObj_->GetRotate();
        recoveryRotation.x += 0.018f;
        recoveryRotation.y += 0.032f;
        recoveryCubeObj_->SetTranslate(recoveryPosition);
        recoveryCubeObj_->SetRotate(recoveryRotation);
        recoveryCubeObj_->Update();

        if (recoveryEffectHandle_ != kInvalidEffectHandle) {
            EffectManager::GetInstance()->SetEffectPosition(
                recoveryEffectHandle_,
                recoveryPosition);
        }
    }

    if (snowEffectHandle_ != kInvalidEffectHandle) {
        EffectManager::GetInstance()->SetEffectPosition(snowEffectHandle_, playerPos_);
    }

    // 左右の手先Joint位置へ炎パーティクルをリアルタイム更新追従
    Vector3 leftHandPos, rightHandPos;
    if (TryGetJointWorldPosition("hand.L", leftHandPos)) {
        if (leftHandFlameHandle_ != kInvalidEffectHandle) {
            EffectManager::GetInstance()->SetEffectPosition(leftHandFlameHandle_, leftHandPos);
        }
    }
    if (TryGetJointWorldPosition("hand.R", rightHandPos)) {
        if (rightHandFlameHandle_ != kInvalidEffectHandle) {
            EffectManager::GetInstance()->SetEffectPosition(rightHandFlameHandle_, rightHandPos);
        }
    }

    EffectManager::GetInstance()->Update();
#if defined(_DEBUG) || defined(USE_IMGUI)
    if (showFieldDebug_) {
        EffectManager::GetInstance()->DrawFieldDebug();
    }
#endif
}

void TestScene1::Draw2D()
{
}

void TestScene1::Draw3D()
{
    // 床の描画
    Object3dManager::GetInstance()->PreDraw();
    LightManager::GetInstance()->Bind(DirectXCommon::GetInstance()->GetCommandList());
    if (floorObj_) {
        floorObj_->Draw();
    }
    if (katanaObj_) {
        katanaObj_->Draw();
    }
    if (recoveryCubeObj_) {
        recoveryCubeObj_->Draw();
    }

    // プレイヤー(Robo)の描画
    SkinningObject3dManager::GetInstance()->PreDraw();
    LightManager::GetInstance()->Bind(DirectXCommon::GetInstance()->GetCommandList());
    if (playerActor_) {
        playerActor_->Draw();
    }
    if (sneakWalkActor_) {
        sneakWalkActor_->Draw();
    }
}

void TestScene1::DrawParticle()
{
    EffectManager::GetInstance()->PreDraw();
    EffectManager::GetInstance()->Draw();
}

void TestScene1::DrawImGui()
{
#ifdef USE_IMGUI
    ImGui::Begin("Test Scene 1 - Robo Controller");
    ImGui::Text("ESCAPE: Return to Title");
    ImGui::Text("Arrow Keys: Move Robo");
    ImGui::Text("Shift + Arrow Keys: Dash");
    ImGui::Text("Left Click Repeatedly: Punch Combo");
    ImGui::Text("WASD/QE: Move Debug Camera");
    ImGui::Text("Right Mouse Drag: Rotate Debug Camera");
    ImGui::Text("F1: Toggle Debug Camera");
    ImGui::Text("SPACE: Jump");
    ImGui::Text("Fixed SneakWalk: skeleton debug display");
    ImGui::Text("Left-side cyan particles: GPU Particle Field demo");
    ImGui::Text("Green cube: HealPickup effect preview");
    ImGui::Checkbox("Show Particle Field Range", &showFieldDebug_);
    ImGui::Text("Mouse Wheel: Change Post Effect");
    ImGui::Text(
        "Post Effect: %s (%u/%u)",
        GetPostEffectTypeName(postEffectTypes_[selectedPostEffectIndex_]),
        static_cast<unsigned int>(selectedPostEffectIndex_ + 1),
        static_cast<unsigned int>(postEffectTypes_.size()));
    ImGui::Separator();
    ImGui::Text("Player Pos: (%.2f, %.2f, %.2f)", playerPos_.x, playerPos_.y, playerPos_.z);
    if (!lastAnimationEventName_.empty()) {
        ImGui::Text("Last Animation Event: %s", lastAnimationEventName_.c_str());
    }
    ImGui::Text("Camera Yaw: %.2f, Pitch: %.2f", cameraYaw_, cameraPitch_);
    ImGui::Separator();
    ImGui::SliderFloat("Player Scale", &playerScale_, 0.1f, 50.0f);
    ImGui::SliderFloat("Player Rot Offset", &playerRotOffset_, -3.1415f, 3.1415f);
    ImGui::SliderFloat("Player Y", &playerPos_.y, -10.0f, 20.0f);
    ImGui::Separator();
    ImGui::Text("Katana Attachment");
    ImGui::SliderFloat("Katana Scale", &katanaScale_, 0.05f, 2.0f);
    ImGui::DragFloat3("Katana Offset", &katanaOffset_.x, 0.01f);
    ImGui::SliderFloat3("Katana Rotation", &katanaRotation_.x, -3.1415f, 3.1415f);
    ImGui::SliderFloat("Camera Distance", &cameraDistance_, 1.0f, 100.0f);
    ImGui::SliderFloat("Camera Pitch", &cameraPitch_, -1.5f, 1.5f);
    ImGui::End();
#endif
}

void TestScene1::Finalize()
{
    StopMovementEffects();
    if (leftHandFlameHandle_ != kInvalidEffectHandle) {
        EffectManager::GetInstance()->StopEffect(leftHandFlameHandle_);
        leftHandFlameHandle_ = kInvalidEffectHandle;
    }
    if (rightHandFlameHandle_ != kInvalidEffectHandle) {
        EffectManager::GetInstance()->StopEffect(rightHandFlameHandle_);
        rightHandFlameHandle_ = kInvalidEffectHandle;
    }
    if (snowEffectHandle_ != kInvalidEffectHandle) {
        EffectManager::GetInstance()->StopEffect(snowEffectHandle_);
        snowEffectHandle_ = kInvalidEffectHandle;
    }
    if (fieldDemoEffectHandle_ != kInvalidEffectHandle) {
        EffectManager::GetInstance()->StopEffect(fieldDemoEffectHandle_);
        fieldDemoEffectHandle_ = kInvalidEffectHandle;
    }
    if (cyberSingularityEffectHandle_ != kInvalidEffectHandle) {
        EffectManager::GetInstance()->StopEffect(cyberSingularityEffectHandle_);
        cyberSingularityEffectHandle_ = kInvalidEffectHandle;
    }
    if (recoveryEffectHandle_ != kInvalidEffectHandle) {
        EffectManager::GetInstance()->StopEffect(recoveryEffectHandle_);
        recoveryEffectHandle_ = kInvalidEffectHandle;
    }
    if (groundLightningOuterHandle_ != kInvalidEffectHandle) {
        EffectManager::GetInstance()->StopEffect(groundLightningOuterHandle_);
        groundLightningOuterHandle_ = kInvalidEffectHandle;
    }
    if (groundLightningInnerHandle_ != kInvalidEffectHandle) {
        EffectManager::GetInstance()->StopEffect(groundLightningInnerHandle_);
        groundLightningInnerHandle_ = kInvalidEffectHandle;
    }
    if (groundLightningMotesHandle_ != kInvalidEffectHandle) {
        EffectManager::GetInstance()->StopEffect(groundLightningMotesHandle_);
        groundLightningMotesHandle_ = kInvalidEffectHandle;
    }

    LightManager* lightManager = LightManager::GetInstance();
    lightManager->SetDirectional(
        { 1.0f, 1.0f, 1.0f, 1.0f },
        { 0.0f, -1.0f, 0.0f },
        1.0f);
    lightManager->SetAmbientColor({ 1.0f, 1.0f, 1.0f });
    lightManager->SetAmbientIntensity(0.25f);
    lightManager->SetPointColor({ 1.0f, 1.0f, 1.0f, 1.0f });
    lightManager->SetPointIntensity(1.0f);
    lightManager->SetPointRadius(10.0f);
    lightManager->SetSpotLightColor({ 1.0f, 1.0f, 1.0f, 1.0f });
    lightManager->SetSpotLightIntensity(4.0f);
    lightManager->SetSpotLightDistance(7.0f);
}

void TestScene1::UpdateKatanaAttachment()
{
    if (!katanaObj_ || !playerActor_ || !playerActor_->GetObject()) {
        return;
    }

    Skeleton* skeleton = playerActor_->GetSkeleton();
    if (!skeleton) {
        return;
    }

    auto handIt = skeleton->jointMap.find("hand.L");
    if (handIt == skeleton->jointMap.end()) {
        return;
    }

    const Joint& handJoint = skeleton->joints[handIt->second];
    Vector3 gripToOrigin = {
        -katanaGripPosition_.x,
        -katanaGripPosition_.y,
        -katanaGripPosition_.z
    };
    Vector3 katanaScale = { katanaScale_, katanaScale_, katanaScale_ };

    Matrix4x4 gripMatrix = MatrixMath::MakeTranslateMatrix(gripToOrigin);
    Matrix4x4 attachmentMatrix = MatrixMath::MakeAffineMatrix(
        katanaScale,
        katanaRotation_,
        katanaOffset_);
    Matrix4x4 katanaLocalMatrix = MatrixMath::Multiply(gripMatrix, attachmentMatrix);
    Matrix4x4 handWorldMatrix = MatrixMath::Multiply(
        handJoint.skeletonSpaceMatrix,
        playerActor_->GetObject()->GetWorldMatrix());
    Matrix4x4 katanaWorldMatrix = MatrixMath::Multiply(katanaLocalMatrix, handWorldMatrix);

    katanaObj_->SetCustomWorldMatrix(katanaWorldMatrix);
    katanaObj_->Update();
}

void TestScene1::ProcessAnimationEvents()
{
    if (!playerActor_ || !playerActor_->GetPlayAnimation()) {
        return;
    }

    AnimationEvent event;
    while (playerActor_->GetPlayAnimation()->PopTriggeredEvent(event)) {
        lastAnimationEventName_ = event.name;
        if (event.value.empty() && event.name != "StopTrail") {
            continue;
        }

        Vector3 eventPosition = playerPos_;
        if (!event.bone.empty()) {
            TryGetJointWorldPosition(event.bone, eventPosition);
        }

        if (event.name == "PlayEffect") {
            if (event.value == "ComboImpact1") {
                EffectManager::GetInstance()->PlayEffect(
                    "NormalBulletImpactFlash",
                    eventPosition);
                EffectManager::GetInstance()->PlayEffect(
                    "GroundLightningPulse",
                    { playerPos_.x, -4.92f, playerPos_.z });
            } else if (event.value == "ComboImpact2") {
                EffectManager::GetInstance()->PlayEffect(
                    "NormalBulletImpactFlash",
                    eventPosition);
                EffectManager::GetInstance()->PlayEffect(
                    "LightningAfterglow",
                    eventPosition);
                EffectManager::GetInstance()->PlayEffect(
                    "ComboLightning",
                    eventPosition);
                EffectManager::GetInstance()->PlayEffect(
                    "GroundLightningPulse",
                    { playerPos_.x, -4.90f, playerPos_.z });
            } else {
                EffectManager::GetInstance()->PlayEffect(
                    event.value,
                    eventPosition);
            }
        } else if (event.name == "PlayKatanaEffect") {
            Vector3 katanaPosition = eventPosition;
            if (katanaObj_) {
                const Matrix4x4& katanaWorld = katanaObj_->GetWorldMatrix();
                katanaPosition = {
                    katanaWorld.m[3][0],
                    katanaWorld.m[3][1],
                    katanaWorld.m[3][2]
                };
            }
            EffectManager::GetInstance()->PlayEffect(
                "UppercutLightning",
                katanaPosition);
            EffectManager::GetInstance()->PlayEffect(
                "GroundLightningPulse",
                { playerPos_.x, -4.88f, playerPos_.z });
            static std::mt19937 lightningRandom(std::random_device {}());
            std::uniform_real_distribution<float> angleDistribution(
                0.0f,
                2.0f * std::numbers::pi_v<float>);
            std::uniform_real_distribution<float> radiusDistribution(
                1.3f,
                4.5f);

            EffectManager::GetInstance()->PlayEffect(
                "FinalStormAfterglow",
                { playerPos_.x, playerPos_.y + 0.2f, playerPos_.z });
            EffectManager::GetInstance()->PlayEffect(
                "FinalStormLightning",
                { playerPos_.x, playerPos_.y + 0.2f, playerPos_.z });
            EffectManager::GetInstance()->PlayEffect(
                "NormalBulletImpactFlash",
                { playerPos_.x, playerPos_.y + 0.8f, playerPos_.z });
            for (int lightningIndex = 0; lightningIndex < 4; ++lightningIndex) {
                float angle = angleDistribution(lightningRandom);
                float radius = radiusDistribution(lightningRandom);
                Vector3 strikePosition = {
                    playerPos_.x + std::cos(angle) * radius,
                    playerPos_.y + 0.1f,
                    playerPos_.z + std::sin(angle) * radius
                };
                EffectManager::GetInstance()->PlayEffect(
                    "FinalStormAfterglow",
                    strikePosition);
                EffectManager::GetInstance()->PlayEffect(
                    "FinalStormLightning",
                    strikePosition);
            }
            EffectManager::GetInstance()->PlayEffect(
                "MissileExplosionFlash",
                katanaPosition);
            EffectManager::GetInstance()->PlayEffect(
                "MissileExplosionRing",
                katanaPosition);
        } else if (event.name == "StartTrail") {
            if (backflipTrailEffectHandle_ != kInvalidEffectHandle) {
                EffectManager::GetInstance()->StopEffect(backflipTrailEffectHandle_);
            }
            backflipTrailEffectHandle_ = EffectManager::GetInstance()->PlayLoopEffect(
                event.value,
                eventPosition);
        } else if (event.name == "StopTrail") {
            if (backflipTrailEffectHandle_ != kInvalidEffectHandle) {
                EffectManager::GetInstance()->StopEffect(backflipTrailEffectHandle_);
                backflipTrailEffectHandle_ = kInvalidEffectHandle;
            }
        }
    }
}

void TestScene1::UpdateMovementEffects()
{
    EffectManager* effectManager = EffectManager::GetInstance();
    Vector3 chestPosition = {
        playerPos_.x,
        playerPos_.y + 2.0f,
        playerPos_.z
    };
    TryGetJointWorldPosition("chest", chestPosition);

    if (currentAnimState_ == PlayerAnimState::Dashing) {
        if (bodySpeedLineEffectHandle_ == kInvalidEffectHandle) {
            bodySpeedLineEffectHandle_ = effectManager->PlayLoopEffect(
                "BodySpeedLines",
                chestPosition);
        }

        if (bodySpeedLineEffectHandle_ != kInvalidEffectHandle) {
            Vector3 backwardVelocity = {
                std::sin(playerRot_.y) * 7.0f,
                0.0f,
                -std::cos(playerRot_.y) * 7.0f
            };
            bool positionUpdated = effectManager->SetEffectPosition(
                bodySpeedLineEffectHandle_,
                chestPosition);
            bool velocityUpdated = effectManager->SetEffectVelocity(
                bodySpeedLineEffectHandle_,
                backwardVelocity);
            if (!positionUpdated || !velocityUpdated) {
                bodySpeedLineEffectHandle_ = kInvalidEffectHandle;
            }
        }
    } else if (bodySpeedLineEffectHandle_ != kInvalidEffectHandle) {
        effectManager->StopEffect(bodySpeedLineEffectHandle_);
        bodySpeedLineEffectHandle_ = kInvalidEffectHandle;
    }

    if (backflipTrailEffectHandle_ != kInvalidEffectHandle) {
        if (currentAnimState_ != PlayerAnimState::CombatIdle) {
            effectManager->StopEffect(backflipTrailEffectHandle_);
            backflipTrailEffectHandle_ = kInvalidEffectHandle;
        } else if (!effectManager->SetEffectPosition(
            backflipTrailEffectHandle_,
            chestPosition)) {
            backflipTrailEffectHandle_ = kInvalidEffectHandle;
        }
    }
}

void TestScene1::PlayDashStartBurst()
{
    EffectManager* effectManager = EffectManager::GetInstance();
    Vector3 burstPosition = {
        playerPos_.x,
        playerPos_.y + 0.15f,
        playerPos_.z
    };
    Vector3 backward = {
        std::sin(playerRot_.y) * 4.5f,
        0.7f,
        -std::cos(playerRot_.y) * 4.5f
    };
    Vector3 right = {
        std::cos(playerRot_.y),
        0.0f,
        std::sin(playerRot_.y)
    };

    for (int smokeIndex = -1; smokeIndex <= 1; ++smokeIndex) {
        Vector3 smokePosition =
            burstPosition + right * (static_cast<float>(smokeIndex) * 0.42f);
        EffectHandle smokeHandle =
            effectManager->PlayEffect("DashDust", smokePosition);
        if (smokeHandle != kInvalidEffectHandle) {
            Vector3 fanVelocity =
                backward + right * (static_cast<float>(smokeIndex) * 2.2f);
            effectManager->SetEffectVelocity(smokeHandle, fanVelocity);
        }
    }

    EffectHandle sparkHandle =
        effectManager->PlayEffect(
            "DashStartSpark",
            burstPosition + Vector3{ 0.0f, 0.32f, 0.0f });
    if (sparkHandle != kInvalidEffectHandle) {
        effectManager->SetEffectVelocity(
            sparkHandle,
            backward * 1.7f + Vector3{ 0.0f, 2.4f, 0.0f });
    }
    effectManager->PlayEffect("DashStartShockwave", burstPosition);
    effectManager->PlayEffect(
        "DashStartShockwaveCore",
        burstPosition + Vector3{ 0.0f, 0.04f, 0.0f });
}

void TestScene1::StopMovementEffects()
{
    EffectManager* effectManager = EffectManager::GetInstance();
    if (bodySpeedLineEffectHandle_ != kInvalidEffectHandle) {
        effectManager->StopEffect(bodySpeedLineEffectHandle_);
        bodySpeedLineEffectHandle_ = kInvalidEffectHandle;
    }
    if (backflipTrailEffectHandle_ != kInvalidEffectHandle) {
        effectManager->StopEffect(backflipTrailEffectHandle_);
        backflipTrailEffectHandle_ = kInvalidEffectHandle;
    }
}

bool TestScene1::TryGetJointWorldPosition(
    const std::string& jointName,
    Vector3& worldPosition) const
{
    if (!playerActor_ || !playerActor_->GetObject()) {
        return false;
    }

    Skeleton* skeleton = playerActor_->GetSkeleton();
    if (!skeleton) {
        return false;
    }

    std::map<std::string, int32_t>::const_iterator jointIterator =
        skeleton->jointMap.find(jointName);
    if (jointIterator == skeleton->jointMap.end()) {
        return false;
    }

    const Joint& joint = skeleton->joints[jointIterator->second];
    const Vector3 localPosition = {
        joint.skeletonSpaceMatrix.m[3][0],
        joint.skeletonSpaceMatrix.m[3][1],
        joint.skeletonSpaceMatrix.m[3][2],
    };
    worldPosition = MatrixMath::Transform(
        localPosition,
        playerActor_->GetObject()->GetWorldMatrix());
    return true;
}

void TestScene1::ApplySelectedPostEffect()
{
    SceneManager* sceneManager = SceneManager::GetInstance();
    sceneManager->ClearPostEffects();
    sceneManager->AddPostEffect(
        postEffectTypes_[selectedPostEffectIndex_],
        PostEffectStage::BeforeParticle);
}

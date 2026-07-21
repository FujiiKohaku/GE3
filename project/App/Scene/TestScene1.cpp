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
    debugCameraController_->SetDebugMode(true);

    // 環境マップの設定
    TextureManager::GetInstance()->LoadTexture("resources/Textures/skybox.dds");
    D3D12_GPU_DESCRIPTOR_HANDLE skyboxHandle = TextureManager::GetInstance()->GetSrvHandleGPU("resources/Textures/skybox.dds");
    Object3dManager::GetInstance()->SetEnvironmentTexture(skyboxHandle);
    SkinningObject3dManager::GetInstance()->SetEnvironmentTexture(skyboxHandle);

    LightManager::GetInstance()->SetDirectional({ 1.0f, 1.0f, 1.0f, 1.0f }, { 0.0f, -1.0f, 0.0f }, 1.0f);

    // floorの初期化
    Model* floorModel = ModelManager::GetInstance()->CreatePlane("resources/Textures/floor_dirt_gemini.jpg", 10.0f, 10.0f);
    floorObj_ = std::make_unique<Object3d>();
    floorObj_->Initialize(Object3dManager::GetInstance());
    floorObj_->SetModel(floorModel);
    floorObj_->SetTranslate({ 0.0f, -5.0f, 0.0f });
    floorObj_->SetRotate({ std::numbers::pi_v<float> / 2.0f, 0.0f, 0.0f });
    floorObj_->SetScale({ 100.0f, 100.0f, 1.0f });

    // Robo Playerの初期化
    const std::string playerModelPath = "Characters/precision_robot_rigged_single_gltf/precision_robot_rigged_single.gltf";
    playerActor_ = std::make_unique<AnimationActor>();
    playerActor_->Initialize(playerModelPath);

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
    katanaObj_->SetModel(katanaModel);

    // Fixed SneakWalk model for skeleton debug display
    sneakWalkActor_ = std::make_unique<AnimationActor>();
    sneakWalkActor_->Initialize("Characters/Animation/SneakWalk/sneakWalk.gltf");
    sneakWalkActor_->SetTranslate({ 7.0f, -5.0f, 0.0f });
    sneakWalkActor_->SetRotate({ 0.0f, std::numbers::pi_v<float>, 0.0f });
    sneakWalkActor_->SetScale({ 2.0f, 2.0f, 2.0f });
    sneakWalkActor_->SetSkeletonDebugVisible(true);

    // 初期アニメーションの設定
    playerActor_->GetPlayAnimation()->SetAnimation(&idleAnimation_);
    currentAnimState_ = PlayerAnimState::Idle;

    // エフェクトマネージャーへのカメラ登録
    EffectManager::GetInstance()->SetCamera(camera_.get());

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
        hasMoveInput = (moveDir.x != 0.0f || moveDir.z != 0.0f);
    }

    bool isDashInput = false;
    if (hasMoveInput) {
        bool isLeftShiftPressed = Input::GetInstance()->IsKeyPressed(DIK_LSHIFT);
        bool isRightShiftPressed = Input::GetInstance()->IsKeyPressed(DIK_RSHIFT);
        isDashInput = isLeftShiftPressed || isRightShiftPressed;
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

        if (Input::GetInstance()->IsMouseTrigger(0)) {
            if (comboStep_ + queuedComboAttacks_ < 2) {
                queuedComboAttacks_++;
            }
        }

        // アニメーションに合わせて手からパーティクルを出す
        if (attackTimer_ >= 0.2f && !hasEmittedParticle_) {
            hasEmittedParticle_ = true;

            // 左右の手、およびビーム発射用ジョイント(BeamSocket)のワールド座標を取得する (純粋なジョイント行列計算)
            Vector3 leftHandPos = playerPos_;
            Vector3 rightHandPos = playerPos_;
            Vector3 beamSocketPos = playerPos_;

            Skeleton* skeleton = playerActor_->GetSkeleton();
            if (skeleton) {
                Matrix4x4 worldMatrix = playerActor_->GetObject()->GetWorldMatrix();

                // 左手
                auto itL = skeleton->jointMap.find("hand.L");
                if (itL != skeleton->jointMap.end()) {
                    const Joint& jointL = skeleton->joints[itL->second];
                    Vector3 localPosL = { jointL.skeletonSpaceMatrix.m[3][0], jointL.skeletonSpaceMatrix.m[3][1], jointL.skeletonSpaceMatrix.m[3][2] };
                    leftHandPos = MatrixMath::Transform(localPosL, worldMatrix);
                }

                // 右手
                auto itR = skeleton->jointMap.find("hand.R");
                if (itR != skeleton->jointMap.end()) {
                    const Joint& jointR = skeleton->joints[itR->second];
                    Vector3 localPosR = { jointR.skeletonSpaceMatrix.m[3][0], jointR.skeletonSpaceMatrix.m[3][1], jointR.skeletonSpaceMatrix.m[3][2] };
                    rightHandPos = MatrixMath::Transform(localPosR, worldMatrix);
                }

                // ビーム発射用ソケット (モデル設定の射撃口ジョイント)
                auto itB = skeleton->jointMap.find("BeamSocket");
                if (itB != skeleton->jointMap.end()) {
                    const Joint& jointB = skeleton->joints[itB->second];
                    Vector3 localPosB = { jointB.skeletonSpaceMatrix.m[3][0], jointB.skeletonSpaceMatrix.m[3][1], jointB.skeletonSpaceMatrix.m[3][2] };
                    beamSocketPos = MatrixMath::Transform(localPosB, worldMatrix);
                }
            }

            // ジョイントの正確な位置からエフェクトを発生 (手から湧き上がる炎エフェクト "HandFlame" に変更)
            EffectManager::GetInstance()->PlayEffect("HandFlame", leftHandPos);
            EffectManager::GetInstance()->PlayEffect("HandFlame", rightHandPos);
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
                hasEmittedParticle_ = false;

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
        if (Input::GetInstance()->IsMouseTrigger(0)) {
            // 攻撃開始 (両手ビームアニメーション)
            currentAnimState_ = PlayerAnimState::Attacking;
            attackTimer_ = 0.0f;
            hasEmittedParticle_ = false;
            comboStep_ = 0;
            queuedComboAttacks_ = 0;
            idleVariationTimer_ = 0.0f;
            combatIdleTimer_ = 0.0f;
            playerActor_->GetPlayAnimation()->SetAnimation(&attackAnimation_, 0.1f);
        }
        else if (Input::GetInstance()->IsKeyTrigger(DIK_SPACE)) {
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

            // 着地後のステート変更
            if (hasMoveInput) {
                if (isDashInput) {
                    currentAnimState_ = PlayerAnimState::Dashing;
                    playerActor_->GetPlayAnimation()->SetAnimation(&dashAnimation_, 0.2f);
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

    debugCameraController_->Update();
    camera_->Update();

    // 更新処理 (止まっている時はアニメーション時間を進めない。ただしブレンド更新中は進める)
    bool isBlending = playerActor_ && playerActor_->GetPlayAnimation() && playerActor_->GetPlayAnimation()->IsBlending();
    float animDeltaTime = 1.0f / 60.0f;
    if (currentAnimState_ == PlayerAnimState::Idle && !isBlending) {
        animDeltaTime = 0.0f;
    }
    playerActor_->Update(animDeltaTime);
    UpdateKatanaAttachment();
    if (sneakWalkActor_) {
        sneakWalkActor_->Update(1.0f / 60.0f);
    }
    if (floorObj_) {
        floorObj_->Update();
    }
    EffectManager::GetInstance()->Update();
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
    ImGui::Text("Mouse Wheel: Change Post Effect");
    ImGui::Text(
        "Post Effect: %s (%u/%u)",
        GetPostEffectTypeName(postEffectTypes_[selectedPostEffectIndex_]),
        static_cast<unsigned int>(selectedPostEffectIndex_ + 1),
        static_cast<unsigned int>(postEffectTypes_.size()));
    ImGui::Separator();
    ImGui::Text("Player Pos: (%.2f, %.2f, %.2f)", playerPos_.x, playerPos_.y, playerPos_.z);
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

void TestScene1::ApplySelectedPostEffect()
{
    SceneManager* sceneManager = SceneManager::GetInstance();
    sceneManager->ClearPostEffects();
    sceneManager->AddPostEffect(postEffectTypes_[selectedPostEffectIndex_]);
}

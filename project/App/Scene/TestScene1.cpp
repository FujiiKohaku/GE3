#include "TestScene1.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/3D/SkinningObject3dManager.h"
#include "Engine/Light/LightManager.h"
#include "Engine/input/Input.h"
#include "TitleScene.h"
#include "externals/imgui/imgui.h"
#include "Engine/3D/ModelManager.h"
#include "Engine/DirectXCommon/DirectXCommon.h"
#include "Engine/Winapp/WinApp.h"
#include "Engine/Animation/AnimationLoder.h"
#include "Engine/TextureManager/TextureManager.h"
#include "Engine/Effect/EffectManager.h"
#include "Engine/Debug/DebugRenderer.h"
#include "Engine/2D/Text/TextRenderer.h"
#include <numbers>
#include <algorithm>
#include <cmath>
#include <random>

namespace {
constexpr float kFootAnkleToSoleDistance = 0.16f;
constexpr const char* kDefaultFont =
    "resources/Fonts/NotoSansJP/NotoSansJP-Variable.ttf";

Vector3 CrossVector(const Vector3& first, const Vector3& second)
{
    return {
        first.y * second.z - first.z * second.y,
        first.z * second.x - first.x * second.z,
        first.x * second.y - first.y * second.x
    };
}

Vector3 TransformDirection(
    const Vector3& direction,
    const Matrix4x4& matrix)
{
    return {
        direction.x * matrix.m[0][0] +
            direction.y * matrix.m[1][0] +
            direction.z * matrix.m[2][0],
        direction.x * matrix.m[0][1] +
            direction.y * matrix.m[1][1] +
            direction.z * matrix.m[2][1],
        direction.x * matrix.m[0][2] +
            direction.y * matrix.m[1][2] +
            direction.z * matrix.m[2][2]
    };
}

Vector3 GetMatrixTranslation(const Matrix4x4& matrix)
{
    return {
        matrix.m[3][0],
        matrix.m[3][1],
        matrix.m[3][2]
    };
}

Quaternion MultiplyQuaternion(
    const Quaternion& first,
    const Quaternion& second)
{
    return {
        first.w * second.x +
            first.x * second.w +
            first.y * second.z -
            first.z * second.y,
        first.w * second.y -
            first.x * second.z +
            first.y * second.w +
            first.z * second.x,
        first.w * second.z +
            first.x * second.y -
            first.y * second.x +
            first.z * second.w,
        first.w * second.w -
            first.x * second.x -
            first.y * second.y -
            first.z * second.z
    };
}

Quaternion QuaternionFromTo(
    const Vector3& fromDirection,
    const Vector3& toDirection)
{
    Vector3 from = NormalizeSafe(fromDirection);
    Vector3 to = NormalizeSafe(toDirection);
    float directionDot = std::clamp(Dot(from, to), -1.0f, 1.0f);

    if (directionDot >= 0.9999f) {
        return { 0.0f, 0.0f, 0.0f, 1.0f };
    }

    if (directionDot <= -0.9999f) {
        Vector3 rotationAxis =
            CrossVector(from, { 1.0f, 0.0f, 0.0f });
        if (Vector3LengthSquared(rotationAxis) <= 0.0001f) {
            rotationAxis =
                CrossVector(from, { 0.0f, 1.0f, 0.0f });
        }
        rotationAxis = NormalizeSafe(rotationAxis);
        return {
            rotationAxis.x,
            rotationAxis.y,
            rotationAxis.z,
            0.0f
        };
    }

    Vector3 rotationAxis = CrossVector(from, to);
    Quaternion rotation = {
        rotationAxis.x,
        rotationAxis.y,
        rotationAxis.z,
        1.0f + directionDot
    };
    return Normalize(rotation);
}

float MoveTowardFloat(
    float currentValue,
    float targetValue,
    float maximumDelta)
{
    if (currentValue < targetValue) {
        const float increasedValue =
            currentValue + maximumDelta;
        if (increasedValue > targetValue) {
            return targetValue;
        }
        return increasedValue;
    }
    if (currentValue > targetValue) {
        const float decreasedValue =
            currentValue - maximumDelta;
        if (decreasedValue < targetValue) {
            return targetValue;
        }
        return decreasedValue;
    }
    return targetValue;
}

float CalculateFootContactWeight(
    float footHeight,
    float surfaceHeight)
{
    const float clearance = footHeight - surfaceHeight;
    const float fullContactClearance = 0.10f;
    const float releasedClearance = 0.45f;
    if (clearance <= fullContactClearance) {
        return 1.0f;
    }
    if (clearance >= releasedClearance) {
        return 0.0f;
    }

    const float releaseRange =
        releasedClearance - fullContactClearance;
    return 1.0f -
        (clearance - fullContactClearance) / releaseRange;
}

float CalculateAnimationFootContactWeight(float liftFromLowestFoot)
{
    const float fullContactLift = 0.04f;
    const float fullReleaseLift = 0.24f;
    if (liftFromLowestFoot <= fullContactLift) {
        return 1.0f;
    }
    if (liftFromLowestFoot >= fullReleaseLift) {
        return 0.0f;
    }

    return 1.0f -
        (liftFromLowestFoot - fullContactLift) /
        (fullReleaseLift - fullContactLift);
}

bool TrySampleTriangleSurface(
    const Vector3& vertexA,
    const Vector3& vertexB,
    const Vector3& vertexC,
    float worldX,
    float worldZ,
    float& surfaceHeight,
    Vector3& surfaceNormal)
{
    const float denominator =
        (vertexB.z - vertexC.z) *
            (vertexA.x - vertexC.x) +
        (vertexC.x - vertexB.x) *
            (vertexA.z - vertexC.z);
    if (std::abs(denominator) <= 0.000001f) {
        return false;
    }

    const float weightA =
        ((vertexB.z - vertexC.z) *
             (worldX - vertexC.x) +
         (vertexC.x - vertexB.x) *
             (worldZ - vertexC.z)) /
        denominator;
    const float weightB =
        ((vertexC.z - vertexA.z) *
             (worldX - vertexC.x) +
         (vertexA.x - vertexC.x) *
             (worldZ - vertexC.z)) /
        denominator;
    const float weightC = 1.0f - weightA - weightB;
    const float tolerance = -0.0001f;
    if (weightA < tolerance ||
        weightB < tolerance ||
        weightC < tolerance) {
        return false;
    }

    surfaceHeight =
        vertexA.y * weightA +
        vertexB.y * weightB +
        vertexC.y * weightC;
    surfaceNormal =
        NormalizeSafe(
            CrossVector(
                vertexB - vertexA,
                vertexC - vertexA));
    if (surfaceNormal.y < 0.0f) {
        surfaceNormal = surfaceNormal * -1.0f;
    }
    return true;
}

void RotateIkJointToward(
    Skeleton& skeleton,
    int32_t jointIndex,
    int32_t effectorIndex,
    const Vector3& targetPosition)
{
    Joint& joint = skeleton.joints[jointIndex];
    const Vector3 jointPosition =
        GetMatrixTranslation(joint.skeletonSpaceMatrix);
    const Vector3 effectorPosition =
        GetMatrixTranslation(
            skeleton.joints[effectorIndex].skeletonSpaceMatrix);
    Vector3 currentDirection = effectorPosition - jointPosition;
    Vector3 targetDirection = targetPosition - jointPosition;
    if (Vector3LengthSquared(currentDirection) <= 0.000001f ||
        Vector3LengthSquared(targetDirection) <= 0.000001f) {
        return;
    }

    Matrix4x4 parentInverse = MatrixMath::MakeIdentity4x4();
    if (joint.parent.has_value()) {
        parentInverse =
            MatrixMath::Inverse(
                skeleton.joints[joint.parent.value()]
                    .skeletonSpaceMatrix);
    }
    currentDirection =
        TransformDirection(currentDirection, parentInverse);
    targetDirection =
        TransformDirection(targetDirection, parentInverse);

    const Quaternion rotationDelta =
        QuaternionFromTo(currentDirection, targetDirection);
    joint.transform.rotate =
        Normalize(
            MultiplyQuaternion(
                rotationDelta,
                joint.transform.rotate));
    skeleton.UpdateSkeleton();
}

void ApplyKneePoleCorrection(
    Skeleton& skeleton,
    int32_t upperIndex,
    int32_t lowerIndex,
    int32_t footIndex,
    const Vector3& animatedKneeDirection)
{
    const Vector3 hipPosition =
        GetMatrixTranslation(
            skeleton.joints[upperIndex].skeletonSpaceMatrix);
    const Vector3 kneePosition =
        GetMatrixTranslation(
            skeleton.joints[lowerIndex].skeletonSpaceMatrix);
    const Vector3 footPosition =
        GetMatrixTranslation(
            skeleton.joints[footIndex].skeletonSpaceMatrix);
    Vector3 legAxis = footPosition - hipPosition;
    if (Vector3LengthSquared(legAxis) <= 0.000001f) {
        return;
    }
    legAxis = NormalizeSafe(legAxis);

    Vector3 currentKneeDirection =
        kneePosition - hipPosition;
    currentKneeDirection =
        currentKneeDirection -
        legAxis * Dot(currentKneeDirection, legAxis);
    Vector3 targetKneeDirection =
        animatedKneeDirection -
        legAxis * Dot(animatedKneeDirection, legAxis);
    if (Vector3LengthSquared(currentKneeDirection) <= 0.000001f ||
        Vector3LengthSquared(targetKneeDirection) <= 0.000001f) {
        return;
    }

    Matrix4x4 parentInverse = MatrixMath::MakeIdentity4x4();
    Joint& upperJoint = skeleton.joints[upperIndex];
    if (upperJoint.parent.has_value()) {
        parentInverse =
            MatrixMath::Inverse(
                skeleton.joints[upperJoint.parent.value()]
                    .skeletonSpaceMatrix);
    }
    currentKneeDirection =
        TransformDirection(
            currentKneeDirection,
            parentInverse);
    targetKneeDirection =
        TransformDirection(
            targetKneeDirection,
            parentInverse);

    const Quaternion poleRotation =
        QuaternionFromTo(
            currentKneeDirection,
            targetKneeDirection);
    upperJoint.transform.rotate =
        Normalize(
            MultiplyQuaternion(
                poleRotation,
                upperJoint.transform.rotate));
    skeleton.UpdateSkeleton();
}
}

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

    ikTerrainModel_ =
        ModelManager::GetInstance()->Load(
            "Environment/Terrain/terrain.obj");
    ikTerrainObj_ = std::make_unique<Object3d>();
    ikTerrainObj_->Initialize(Object3dManager::GetInstance());
    ikTerrainObj_->SetEnableLighting(true);
    ikTerrainObj_->SetModel(ikTerrainModel_);
    ikTerrainObj_->SetTranslate(ikTerrainPosition_);
    ikTerrainObj_->SetScale(ikTerrainScale_);
    ikTerrainObj_->Update();

    Model* ikTestBlockModel =
        ModelManager::GetInstance()->Load(
            "Environment/Block/block.obj");
    for (size_t blockIndex = 0;
         blockIndex < kIkTestBlockCount;
         ++blockIndex) {
        ikTestBlockObjs_[blockIndex] =
            std::make_unique<Object3d>();
        ikTestBlockObjs_[blockIndex]->Initialize(
            Object3dManager::GetInstance());
        ikTestBlockObjs_[blockIndex]->SetModel(
            ikTestBlockModel);
        ikTestBlockObjs_[blockIndex]->SetTranslate(
            ikTestBlockPositions_[blockIndex]);
        ikTestBlockObjs_[blockIndex]->SetScale(
            ikTestBlockScales_[blockIndex]);
        ikTestBlockObjs_[blockIndex]->SetRotate(
            ikTestBlockRotations_[blockIndex]);
        ikTestBlockObjs_[blockIndex]->SetEnableLighting(true);
        ikTestBlockObjs_[blockIndex]->SetColor(
            { 0.34f, 0.43f, 0.52f, 1.0f });
        ikTestBlockObjs_[blockIndex]->Update();
    }

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
    cyberSingularityDebrisHandle_ =
        EffectManager::GetInstance()->PlayLoopEffect(
            "CyberSingularityDebris",
            { 0.0f, -2.0f, 8.0f });
    cyberSingularityJetsHandle_ =
        EffectManager::GetInstance()->PlayLoopEffect(
            "CyberSingularityJets",
            { 0.0f, -2.0f, 8.0f });
    sandstormGolemEffectHandle_ = kInvalidEffectHandle;
    isSandGolemMode_ = false;
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

    hinokamiFlameHandle_ = EffectManager::GetInstance()->PlayLoopEffect("HinokamiFlame", playerPos_);
    hinokamiEmbersHandle_ = EffectManager::GetInstance()->PlayLoopEffect("HinokamiEmbers", playerPos_);

    selectedPostEffectIndex_ = 0;
    ApplySelectedPostEffect();

    // ジョイント名テキストの初期化
    Skeleton* skeleton = playerActor_->GetSkeleton();
    if (skeleton) {
        for (const Joint& joint : skeleton->joints) {
            auto text = std::make_unique<Text>();
            text->Initialize(kDefaultFont);
            text->SetText(joint.name);
            text->SetFontSize(14.0f);
            text->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
            text->SetOutlineColor({ 0.0f, 0.0f, 0.0f, 1.0f });
            text->SetOutlineWidth(1.0f);
            jointNameTexts_.push_back(std::move(text));
        }
    }
}

void TestScene1::Update()
{
    // タイトルシーンに戻る
    if (Input::GetInstance()->IsKeyTrigger(DIK_ESCAPE)) {
        SceneManager::GetInstance()->SetNextScene(std::make_unique<TitleScene>());
        return;
    }

    if (Input::GetInstance()->IsKeyTrigger(DIK_F4)) {
        showSkeletonDebug_ = !showSkeletonDebug_;
        if (playerActor_) {
            playerActor_->SetSkeletonDebugVisible(showSkeletonDebug_);
        }
        if (sneakWalkActor_) {
            sneakWalkActor_->SetSkeletonDebugVisible(showSkeletonDebug_);
        }
    }

    // 【1キー発火】演出終了まで再発動不可ガード（連打防止）
    if (Input::GetInstance()->IsKeyTrigger(DIK_1) && !isSequenceActive_) {
        isSequenceActive_ = true;
        coolDownTimer_ = 2.4f; // 一連の二重破裂演出が完了するまでの時間（2.4秒ガード）

        EffectManager::GetInstance()->PlayEffect("IceGroundPattern", playerPos_);
        EffectManager::GetInstance()->PlayEffect("IceSpikes", playerPos_);

        sixSeqCenterPos_ = playerPos_;

        // 0.28秒後に6方向へ花火が飛び出す
        isSixTrailPending_ = true;
        sixTrailTimer_ = 0.28f;

        // 0.72秒後に1回目の大爆発！
        isSixBlastPending_ = true;
        sixBlastTimer_ = 0.72f;

        // 0.98秒後に追っかけ2回目の【二重破裂】大爆発！
        isSecondBlastPending_ = true;
        secondBlastTimer_ = 0.98f;
    }

    // 【2キー発火】Field機能（Vortex 竜巻渦 ＋ Wind 上昇気流フィールド）の実戦デモ発動！
    if (Input::GetInstance()->IsKeyTrigger(DIK_2)) {
        EffectManager::GetInstance()->PlayEffect("VortexTornado", playerPos_);
    }

    if (Input::GetInstance()->IsKeyTrigger(DIK_3)) {
        if (isSandGolemMode_) {
            isSandGolemMode_ = false;
            if (sandstormGolemEffectHandle_ != kInvalidEffectHandle) {
                EffectManager::GetInstance()->StopEffect(
                    sandstormGolemEffectHandle_);
                sandstormGolemEffectHandle_ = kInvalidEffectHandle;
            }
        } else {
            isSandGolemMode_ = true;
            sandstormGolemEffectHandle_ =
                EffectManager::GetInstance()->PlayEffect(
                    "SandstormGolem",
                    playerPos_);
        }
    }

    // クールダウン・演出実行中ガードのタイマー更新
    if (isSequenceActive_) {
        coolDownTimer_ -= 1.0f / 60.0f;
        if (coolDownTimer_ <= 0.0f) {
            isSequenceActive_ = false;
        }
    }

    // 1. 氷の中心から6方向への花火飛翔発射
    if (isSixTrailPending_) {
        sixTrailTimer_ -= 1.0f / 60.0f;
        if (sixTrailTimer_ <= 0.0f) {
            isSixTrailPending_ = false;
            EffectManager::GetInstance()->PlayEffect("SixDirectionFireworkTrails", sixSeqCenterPos_);
        }
    }

    // 2. 【第1弾爆発】6方向の到達地点（＋中央）の全7箇所での3重連動大爆発！
    if (isSixBlastPending_) {
        sixBlastTimer_ -= 1.0f / 60.0f;
        if (sixBlastTimer_ <= 0.0f) {
            isSixBlastPending_ = false;

            // ① 中央大爆発
            EffectManager::GetInstance()->PlayEffect("BlueSilverBlast", sixSeqCenterPos_);
            EffectManager::GetInstance()->PlayEffect("DeepBlueCore", sixSeqCenterPos_);
            EffectManager::GetInstance()->PlayEffect("OrangeEmberCore", sixSeqCenterPos_);

            // ② 6方向到達地点での大爆発（距離11.5m・高度6.5m）
            const float kPi = 3.14159265f;
            const float dist = 11.5f;
            const float height = 6.5f;

            for (int i = 0; i < 6; ++i) {
                float angle = (float)i * (kPi / 3.0f);
                Vector3 blastPos = {
                    sixSeqCenterPos_.x + cosf(angle) * dist,
                    sixSeqCenterPos_.y + height,
                    sixSeqCenterPos_.z + sinf(angle) * dist
                };
                EffectManager::GetInstance()->PlayEffect("BlueSilverBlast", blastPos);
                EffectManager::GetInstance()->PlayEffect("DeepBlueCore", blastPos);
                EffectManager::GetInstance()->PlayEffect("OrangeEmberCore", blastPos);
            }
        }
    }

    // 3. 【第2弾二重破裂】第1弾爆発の直後に追いかけてドガガガーン！と広範囲で2度目の二重破裂！
    if (isSecondBlastPending_) {
        secondBlastTimer_ -= 1.0f / 60.0f;
        if (secondBlastTimer_ <= 0.0f) {
            isSecondBlastPending_ = false;

            // ① 中央の追っかけ二重破裂（上空高め）
            Vector3 centerHigh = { sixSeqCenterPos_.x, sixSeqCenterPos_.y + 9.5f, sixSeqCenterPos_.z };
            EffectManager::GetInstance()->PlayEffect("BlueSilverBlast", centerHigh);
            EffectManager::GetInstance()->PlayEffect("OrangeEmberCore", centerHigh);

            // ② 6方向の外側さらに広い範囲での追っかけ二重破裂（距離16.0m・高度10.5m）
            const float kPi = 3.14159265f;
            const float dist2 = 16.0f;
            const float height2 = 10.5f;

            for (int i = 0; i < 6; ++i) {
                float angle = (float)i * (kPi / 3.0f) + 0.523598f; // 30度オフセットで交差破裂
                Vector3 blastPos2 = {
                    sixSeqCenterPos_.x + cosf(angle) * dist2,
                    sixSeqCenterPos_.y + height2,
                    sixSeqCenterPos_.z + sinf(angle) * dist2
                };
                EffectManager::GetInstance()->PlayEffect("BlueSilverBlast", blastPos2);
                EffectManager::GetInstance()->PlayEffect("DeepBlueCore", blastPos2);
                EffectManager::GetInstance()->PlayEffect("OrangeEmberCore", blastPos2);
            }
        }
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
    float playerGroundHeight = -5.0f;
    const float facingYaw =
        playerRot_.y - playerRotOffset_;
    const Vector3 playerRight = {
        std::cos(facingYaw),
        0.0f,
        std::sin(facingYaw)
    };
    const Vector3 leftGroundProbe =
        playerPos_ - playerRight * 0.28f;
    const Vector3 rightGroundProbe =
        playerPos_ + playerRight * 0.28f;
    float leftGroundHeight = 0.0f;
    float rightGroundHeight = 0.0f;
    Vector3 leftGroundNormal {};
    Vector3 rightGroundNormal {};
    const bool hasLeftGround =
        SampleTerrainSurface(
            leftGroundProbe.x,
            leftGroundProbe.z,
            leftGroundHeight,
            leftGroundNormal);
    const bool hasRightGround =
        SampleTerrainSurface(
            rightGroundProbe.x,
            rightGroundProbe.z,
            rightGroundHeight,
            rightGroundNormal);
    if (hasLeftGround && hasRightGround) {
        playerGroundHeight =
            (leftGroundHeight + rightGroundHeight) * 0.5f;
    } else if (hasLeftGround) {
        playerGroundHeight = leftGroundHeight;
    } else if (hasRightGround) {
        playerGroundHeight = rightGroundHeight;
    } else {
        Vector3 centerGroundNormal {};
        SampleTerrainSurface(
            playerPos_.x,
            playerPos_.z,
            playerGroundHeight,
            centerGroundNormal);
    }

    if (!isJumping_) {
        float groundFollowDelta = 0.10f;
        if (playerGroundHeight > playerPos_.y) {
            groundFollowDelta = 0.25f;
        }
        playerPos_.y =
            MoveTowardFloat(
                playerPos_.y,
                playerGroundHeight,
                groundFollowDelta);
    }

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
        if (playerPos_.y <= playerGroundHeight) {
            playerPos_.y = playerGroundHeight;
            isJumping_ = false;

            EffectManager::GetInstance()->PlayEffect(
                "LandingDust",
                {
                    playerPos_.x,
                    playerGroundHeight + 0.15f,
                    playerPos_.z
                });

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
        playerPos_.x,
        playerGroundHeight + 0.06f,
        playerPos_.z
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

    if (isSandGolemMode_) {
        if (sandstormGolemEffectHandle_ != kInvalidEffectHandle &&
            !EffectManager::GetInstance()->SetEffectPosition(
                sandstormGolemEffectHandle_,
                playerPos_)) {
            sandstormGolemEffectHandle_ = kInvalidEffectHandle;
        }
    }
    camera_->Update();

    const Vector3 blackHoleWorldPosition = { 0.0f, -2.0f, 8.0f };
    const Vector2 blackHoleScreenPosition =
        camera_->WorldToScreen(blackHoleWorldPosition);
    float clientWidth =
        static_cast<float>(WinApp::GetInstance()->GetClientWidth());
    float clientHeight =
        static_cast<float>(WinApp::GetInstance()->GetClientHeight());
    if (clientWidth <= 0.0f) {
        clientWidth = static_cast<float>(WinApp::kClientWidth);
    }
    if (clientHeight <= 0.0f) {
        clientHeight = static_cast<float>(WinApp::kClientHeight);
    }
    SceneManager::GetInstance()->SetBlackHoleCenter({
        blackHoleScreenPosition.x / clientWidth,
        blackHoleScreenPosition.y / clientHeight
    });

    // 更新処理 (止まっている時はアニメーション時間を進めない。ただしブレンド更新中は進める)
    bool isBlending = playerActor_ && playerActor_->GetPlayAnimation() && playerActor_->GetPlayAnimation()->IsBlending();
    float animDeltaTime = 1.0f / 60.0f;
    if (currentAnimState_ == PlayerAnimState::Idle && !isBlending) {
        animDeltaTime = 0.0f;
    }
    playerActor_->Update(animDeltaTime);
    ApplyFootIK();
    UpdateSandGolemSkeletonPose();
    ProcessAnimationEvents();
    UpdateMovementEffects();
    UpdateKatanaAttachment();
    UpdateBoneNames();
    if (sneakWalkActor_) {
        sneakWalkActor_->Update(1.0f / 60.0f);
    }
    if (floorObj_) {
        floorObj_->Update();
    }
    if (ikTerrainObj_) {
        ikTerrainObj_->Update();
    }
    for (size_t blockIndex = 0;
         blockIndex < kIkTestBlockCount;
         ++blockIndex) {
        if (ikTestBlockObjs_[blockIndex]) {
            ikTestBlockObjs_[blockIndex]->Update();
        }
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

        // ヒノカミ神楽（炎の龍・火の粉）を右手の刀位置にリアルタイム追従
        if (hinokamiFlameHandle_ != kInvalidEffectHandle) {
            EffectManager::GetInstance()->SetEffectPosition(hinokamiFlameHandle_, rightHandPos);
        }
        if (hinokamiEmbersHandle_ != kInvalidEffectHandle) {
            EffectManager::GetInstance()->SetEffectPosition(hinokamiEmbersHandle_, rightHandPos);
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
    if (!showSkeletonDebug_) {
        return;
    }

    // 骨の名前テキストを描画
    TextRenderer::GetInstance()->PreDraw();
    for (auto& text : jointNameTexts_) {
        text->Draw();
    }
}

void TestScene1::Draw3D()
{
    // 床の描画
    Object3dManager::GetInstance()->PreDraw();
    LightManager::GetInstance()->Bind(DirectXCommon::GetInstance()->GetCommandList());
    if (floorObj_) {
        floorObj_->Draw();
    }
    if (ikTerrainObj_) {
        ikTerrainObj_->Draw();
    }
    for (size_t blockIndex = 0;
         blockIndex < kIkTestBlockCount;
         ++blockIndex) {
        if (ikTestBlockObjs_[blockIndex]) {
            ikTestBlockObjs_[blockIndex]->Draw();
        }
    }
    if (katanaObj_ && !isSandGolemMode_) {
        katanaObj_->Draw();
    }
    if (recoveryCubeObj_) {
        recoveryCubeObj_->Draw();
    }

    // プレイヤー(Robo)の描画
    SkinningObject3dManager::GetInstance()->PreDraw();
    LightManager::GetInstance()->Bind(DirectXCommon::GetInstance()->GetCommandList());
    if (playerActor_ && !isSandGolemMode_) {
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
    ImGui::Text("3: Toggle Sand Golem Form");
    if (isSandGolemMode_) {
        ImGui::Text("Current Form: Sand Golem");
    } else {
        ImGui::Text("Current Form: Robot");
    }
    ImGui::Checkbox("Enable Foot IK", &enableFootIK_);
    ImGui::Checkbox("Show Foot IK Debug", &showFootIKDebug_);
    ImGui::Text(
        "Foot IK Weight L: %.2f  R: %.2f",
        leftFootIkWeight_,
        rightFootIkWeight_);
    ImGui::SliderFloat(
        "Foot Sole Ground Margin",
        &footSoleGroundMargin_,
        0.0f,
        0.30f,
        "%.3f");
    ImGui::Text("WASD/QE: Move Debug Camera");
    ImGui::Text("Right Mouse Drag: Rotate Debug Camera");
    ImGui::Text("F1: Toggle Debug Camera");
    ImGui::Text("F4: Toggle Skeleton Debug");
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
    if (hinokamiFlameHandle_ != kInvalidEffectHandle) {
        EffectManager::GetInstance()->StopEffect(hinokamiFlameHandle_);
        hinokamiFlameHandle_ = kInvalidEffectHandle;
    }
    if (hinokamiEmbersHandle_ != kInvalidEffectHandle) {
        EffectManager::GetInstance()->StopEffect(hinokamiEmbersHandle_);
        hinokamiEmbersHandle_ = kInvalidEffectHandle;
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
    if (cyberSingularityDebrisHandle_ != kInvalidEffectHandle) {
        EffectManager::GetInstance()->StopEffect(
            cyberSingularityDebrisHandle_);
        cyberSingularityDebrisHandle_ = kInvalidEffectHandle;
    }
    if (cyberSingularityJetsHandle_ != kInvalidEffectHandle) {
        EffectManager::GetInstance()->StopEffect(
            cyberSingularityJetsHandle_);
        cyberSingularityJetsHandle_ = kInvalidEffectHandle;
    }
    if (sandstormGolemEffectHandle_ != kInvalidEffectHandle) {
        EffectManager::GetInstance()->StopEffect(
            sandstormGolemEffectHandle_);
        sandstormGolemEffectHandle_ = kInvalidEffectHandle;
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

bool TestScene1::SampleTerrainSurface(
    float worldX,
    float worldZ,
    float& worldHeight,
    Vector3& worldNormal) const
{
    if (!ikTerrainModel_) {
        return false;
    }

    bool foundSurface = false;
    float highestSurface = 0.0f;
    Vector3 highestNormal = { 0.0f, 1.0f, 0.0f };
    const ModelData& modelData = ikTerrainModel_->GetModelData();

    for (const MeshPrimitive& primitive : modelData.primitives) {
        if (primitive.mode != PrimitiveMode::Triangles) {
            continue;
        }

        for (size_t indexOffset = 0;
             indexOffset + 2 < primitive.indices.size();
             indexOffset += 3) {
            const uint32_t indexA = primitive.indices[indexOffset];
            const uint32_t indexB = primitive.indices[indexOffset + 1];
            const uint32_t indexC = primitive.indices[indexOffset + 2];
            if (indexA >= primitive.vertices.size() ||
                indexB >= primitive.vertices.size() ||
                indexC >= primitive.vertices.size()) {
                continue;
            }

            const Vector4& sourceA =
                primitive.vertices[indexA].position;
            const Vector4& sourceB =
                primitive.vertices[indexB].position;
            const Vector4& sourceC =
                primitive.vertices[indexC].position;
            const Vector3 vertexA = {
                sourceA.x * ikTerrainScale_.x + ikTerrainPosition_.x,
                sourceA.y * ikTerrainScale_.y + ikTerrainPosition_.y,
                sourceA.z * ikTerrainScale_.z + ikTerrainPosition_.z
            };
            const Vector3 vertexB = {
                sourceB.x * ikTerrainScale_.x + ikTerrainPosition_.x,
                sourceB.y * ikTerrainScale_.y + ikTerrainPosition_.y,
                sourceB.z * ikTerrainScale_.z + ikTerrainPosition_.z
            };
            const Vector3 vertexC = {
                sourceC.x * ikTerrainScale_.x + ikTerrainPosition_.x,
                sourceC.y * ikTerrainScale_.y + ikTerrainPosition_.y,
                sourceC.z * ikTerrainScale_.z + ikTerrainPosition_.z
            };

            const float denominator =
                (vertexB.z - vertexC.z) *
                    (vertexA.x - vertexC.x) +
                (vertexC.x - vertexB.x) *
                    (vertexA.z - vertexC.z);
            if (std::abs(denominator) <= 0.000001f) {
                continue;
            }

            const float weightA =
                ((vertexB.z - vertexC.z) *
                     (worldX - vertexC.x) +
                 (vertexC.x - vertexB.x) *
                     (worldZ - vertexC.z)) /
                denominator;
            const float weightB =
                ((vertexC.z - vertexA.z) *
                     (worldX - vertexC.x) +
                 (vertexA.x - vertexC.x) *
                     (worldZ - vertexC.z)) /
                denominator;
            const float weightC = 1.0f - weightA - weightB;
            const float tolerance = -0.0001f;
            if (weightA < tolerance ||
                weightB < tolerance ||
                weightC < tolerance) {
                continue;
            }

            const float surfaceHeight =
                vertexA.y * weightA +
                vertexB.y * weightB +
                vertexC.y * weightC;
            if (foundSurface && surfaceHeight <= highestSurface) {
                continue;
            }

            Vector3 surfaceNormal =
                NormalizeSafe(
                    CrossVector(
                        vertexB - vertexA,
                        vertexC - vertexA));
            if (surfaceNormal.y < 0.0f) {
                surfaceNormal = surfaceNormal * -1.0f;
            }
            highestSurface = surfaceHeight;
            highestNormal = surfaceNormal;
            foundSurface = true;
        }
    }

    for (size_t blockIndex = 0;
         blockIndex < kIkTestBlockCount;
         ++blockIndex) {
        if (!ikTestBlockObjs_[blockIndex]) {
            continue;
        }

        const Matrix4x4& blockWorldMatrix =
            ikTestBlockObjs_[blockIndex]->GetWorldMatrix();
        const Vector3 topCornerA =
            MatrixMath::Transform(
                { -0.5f, 0.5f, -0.5f },
                blockWorldMatrix);
        const Vector3 topCornerB =
            MatrixMath::Transform(
                { 0.5f, 0.5f, -0.5f },
                blockWorldMatrix);
        const Vector3 topCornerC =
            MatrixMath::Transform(
                { 0.5f, 0.5f, 0.5f },
                blockWorldMatrix);
        const Vector3 topCornerD =
            MatrixMath::Transform(
                { -0.5f, 0.5f, 0.5f },
                blockWorldMatrix);

        float blockHeight = 0.0f;
        Vector3 blockNormal {};
        bool foundBlockSurface =
            TrySampleTriangleSurface(
                topCornerA,
                topCornerB,
                topCornerC,
                worldX,
                worldZ,
                blockHeight,
                blockNormal);
        if (!foundBlockSurface) {
            foundBlockSurface =
                TrySampleTriangleSurface(
                    topCornerA,
                    topCornerC,
                    topCornerD,
                    worldX,
                    worldZ,
                    blockHeight,
                    blockNormal);
        }
        if (!foundBlockSurface) {
            continue;
        }
        if (foundSurface && blockHeight <= highestSurface) {
            continue;
        }

        highestSurface = blockHeight;
        highestNormal = blockNormal;
        foundSurface = true;
    }

    if (!foundSurface) {
        return false;
    }

    worldHeight = highestSurface;
    worldNormal = highestNormal;
    return true;
}

bool TestScene1::SampleFootSoleSurface(
    const Vector3& footJointPosition,
    float& worldHeight,
    Vector3& worldNormal) const
{
    const float facingYaw =
        playerRot_.y - playerRotOffset_;
    const Vector3 footForward = {
        -std::sin(facingYaw),
        0.0f,
        std::cos(facingYaw)
    };
    const Vector3 footRight = {
        std::cos(facingYaw),
        0.0f,
        std::sin(facingYaw)
    };
    const Vector3 heelPosition =
        footJointPosition - footForward * 0.20f;
    const Vector3 toePosition =
        footJointPosition + footForward * 0.32f;

    float heelHeight = 0.0f;
    float toeHeight = 0.0f;
    Vector3 heelNormal {};
    Vector3 toeNormal {};
    const bool hasHeelSurface =
        SampleTerrainSurface(
            heelPosition.x,
            heelPosition.z,
            heelHeight,
            heelNormal);
    const bool hasToeSurface =
        SampleTerrainSurface(
            toePosition.x,
            toePosition.z,
            toeHeight,
            toeNormal);

    if (hasHeelSurface && hasToeSurface) {
        worldHeight = (heelHeight + toeHeight) * 0.5f;
        const Vector3 forwardSlope =
            NormalizeSafe({
                toePosition.x - heelPosition.x,
                toeHeight - heelHeight,
                toePosition.z - heelPosition.z
            });
        Vector3 slopeNormal =
            NormalizeSafe(
                CrossVector(
                    forwardSlope,
                    footRight));
        if (slopeNormal.y < 0.0f) {
            slopeNormal = slopeNormal * -1.0f;
        }
        const Vector3 sampledNormal =
            NormalizeSafe(heelNormal + toeNormal);
        worldNormal =
            NormalizeSafe(slopeNormal + sampledNormal);
        return true;
    }

    if (hasHeelSurface) {
        worldHeight = heelHeight;
        worldNormal = heelNormal;
        return true;
    }
    if (hasToeSurface) {
        worldHeight = toeHeight;
        worldNormal = toeNormal;
        return true;
    }
    return false;
}

void TestScene1::SolveLegIK(
    const std::string& upperLegName,
    const std::string& lowerLegName,
    const std::string& footName,
    const Vector3& worldTarget,
    const Vector3& worldNormal,
    float ikWeight)
{
    if (!playerActor_ || !playerActor_->GetObject()) {
        return;
    }

    Skeleton* skeleton = playerActor_->GetSkeleton();
    if (!skeleton) {
        return;
    }

    const std::map<std::string, int32_t>::const_iterator upperIterator =
        skeleton->jointMap.find(upperLegName);
    const std::map<std::string, int32_t>::const_iterator lowerIterator =
        skeleton->jointMap.find(lowerLegName);
    const std::map<std::string, int32_t>::const_iterator footIterator =
        skeleton->jointMap.find(footName);
    if (upperIterator == skeleton->jointMap.end() ||
        lowerIterator == skeleton->jointMap.end() ||
        footIterator == skeleton->jointMap.end()) {
        return;
    }

    const int32_t upperIndex = upperIterator->second;
    const int32_t lowerIndex = lowerIterator->second;
    const int32_t footIndex = footIterator->second;
    ikWeight = std::clamp(ikWeight, 0.0f, 1.0f);
    if (ikWeight <= 0.0001f) {
        return;
    }

    const Quaternion originalUpperRotation =
        skeleton->joints[upperIndex].transform.rotate;
    const Quaternion originalLowerRotation =
        skeleton->joints[lowerIndex].transform.rotate;
    const Quaternion originalFootRotation =
        skeleton->joints[footIndex].transform.rotate;
    const Vector3 originalHipPosition =
        GetMatrixTranslation(
            skeleton->joints[upperIndex].skeletonSpaceMatrix);
    const Vector3 originalKneePosition =
        GetMatrixTranslation(
            skeleton->joints[lowerIndex].skeletonSpaceMatrix);
    const Vector3 animatedKneeDirection =
        originalKneePosition - originalHipPosition;
    const Matrix4x4 inverseWorld =
        MatrixMath::Inverse(
            playerActor_->GetObject()->GetWorldMatrix());
    const Vector3 skeletonTarget =
        MatrixMath::Transform(worldTarget, inverseWorld);
    const Vector3 skeletonNormal =
        NormalizeSafe(
            TransformDirection(worldNormal, inverseWorld));
    const Vector3 skeletonWorldUp =
        NormalizeSafe(
            TransformDirection(
                { 0.0f, 1.0f, 0.0f },
                inverseWorld));

    for (int iteration = 0; iteration < 3; ++iteration) {
        RotateIkJointToward(
            *skeleton,
            lowerIndex,
            footIndex,
            skeletonTarget);
        RotateIkJointToward(
            *skeleton,
            upperIndex,
            footIndex,
            skeletonTarget);
    }
    ApplyKneePoleCorrection(
        *skeleton,
        upperIndex,
        lowerIndex,
        footIndex,
        animatedKneeDirection);

    Joint& footJoint = skeleton->joints[footIndex];
    Matrix4x4 parentInverse = MatrixMath::MakeIdentity4x4();
    if (footJoint.parent.has_value()) {
        parentInverse =
            MatrixMath::Inverse(
                skeleton->joints[footJoint.parent.value()]
                    .skeletonSpaceMatrix);
    }
    const Vector3 currentFootUp =
        TransformDirection(
            skeletonWorldUp,
            parentInverse);
    Vector3 targetFootUp =
        TransformDirection(skeletonNormal, parentInverse);
    const Quaternion footRotationDelta =
        QuaternionFromTo(currentFootUp, targetFootUp);
    footJoint.transform.rotate =
        Normalize(
            MultiplyQuaternion(
                footRotationDelta,
                footJoint.transform.rotate));

    const Quaternion solvedUpperRotation =
        skeleton->joints[upperIndex].transform.rotate;
    const Quaternion solvedLowerRotation =
        skeleton->joints[lowerIndex].transform.rotate;
    const Quaternion solvedFootRotation =
        skeleton->joints[footIndex].transform.rotate;
    skeleton->joints[upperIndex].transform.rotate =
        Slerp(
            originalUpperRotation,
            solvedUpperRotation,
            ikWeight);
    skeleton->joints[lowerIndex].transform.rotate =
        Slerp(
            originalLowerRotation,
            solvedLowerRotation,
            ikWeight);
    skeleton->joints[footIndex].transform.rotate =
        Slerp(
            originalFootRotation,
            solvedFootRotation,
            ikWeight * 0.75f);
    skeleton->UpdateSkeleton();
}

void TestScene1::ApplyFootIK()
{
    if (!enableFootIK_ ||
        isJumping_ ||
        !playerActor_ ||
        !playerActor_->GetObject() ||
        !ikTerrainModel_) {
        leftFootIkWeight_ = 0.0f;
        rightFootIkWeight_ = 0.0f;
        return;
    }

    Vector3 leftFootPosition {};
    Vector3 rightFootPosition {};
    if (!TryGetJointWorldPosition("foot.L", leftFootPosition) ||
        !TryGetJointWorldPosition("foot.R", rightFootPosition)) {
        return;
    }

    float leftHeight = 0.0f;
    float rightHeight = 0.0f;
    Vector3 leftNormal {};
    Vector3 rightNormal {};
    const bool hasLeftSurface =
        SampleFootSoleSurface(
            leftFootPosition,
            leftHeight,
            leftNormal);
    const bool hasRightSurface =
        SampleFootSoleSurface(
            rightFootPosition,
            rightHeight,
            rightNormal);

    float leftTargetWeight = 0.0f;
    float rightTargetWeight = 0.0f;
    const float ankleToGroundDistance =
        kFootAnkleToSoleDistance +
        footSoleGroundMargin_;
    if (hasLeftSurface) {
        leftTargetWeight =
            CalculateFootContactWeight(
                leftFootPosition.y -
                    ankleToGroundDistance,
                leftHeight);
    }
    if (hasRightSurface) {
        rightTargetWeight =
            CalculateFootContactWeight(
                rightFootPosition.y -
                    ankleToGroundDistance,
                rightHeight);
    }
    if (currentAnimState_ == PlayerAnimState::Running ||
        currentAnimState_ == PlayerAnimState::Dashing) {
        float lowestAnimatedFootHeight =
            leftFootPosition.y;
        if (rightFootPosition.y < lowestAnimatedFootHeight) {
            lowestAnimatedFootHeight = rightFootPosition.y;
        }
        const float leftAnimationContact =
            CalculateAnimationFootContactWeight(
                leftFootPosition.y -
                lowestAnimatedFootHeight);
        const float rightAnimationContact =
            CalculateAnimationFootContactWeight(
                rightFootPosition.y -
                lowestAnimatedFootHeight);
        leftTargetWeight *= leftAnimationContact;
        rightTargetWeight *= rightAnimationContact;
    }
    leftFootIkWeight_ =
        MoveTowardFloat(
            leftFootIkWeight_,
            leftTargetWeight,
            0.18f);
    rightFootIkWeight_ =
        MoveTowardFloat(
            rightFootIkWeight_,
            rightTargetWeight,
            0.18f);

    float activeLeftWeight = 0.0f;
    float activeRightWeight = 0.0f;
    if (hasLeftSurface) {
        activeLeftWeight = leftFootIkWeight_;
    }
    if (hasRightSurface) {
        activeRightWeight = rightFootIkWeight_;
    }
    const float totalContactWeight =
        activeLeftWeight + activeRightWeight;
    if (totalContactWeight > 0.0001f) {
        const float leftHeightDifference =
            leftHeight +
            ankleToGroundDistance -
            leftFootPosition.y;
        const float rightHeightDifference =
            rightHeight +
            ankleToGroundDistance -
            rightFootPosition.y;
        float pelvisWorldOffset =
            (leftHeightDifference * activeLeftWeight +
                rightHeightDifference * activeRightWeight) /
            totalContactWeight;
        pelvisWorldOffset =
            std::clamp(pelvisWorldOffset, -0.35f, 0.35f);
        pelvisWorldOffset *= 0.45f;

        Skeleton* skeleton = playerActor_->GetSkeleton();
        const std::map<std::string, int32_t>::const_iterator pelvisIterator =
            skeleton->jointMap.find("pelvis");
        if (pelvisIterator != skeleton->jointMap.end()) {
            const Matrix4x4 inverseWorld =
                MatrixMath::Inverse(
                    playerActor_->GetObject()->GetWorldMatrix());
            const Vector3 localPelvisOffset =
                TransformDirection(
                    { 0.0f, pelvisWorldOffset, 0.0f },
                    inverseWorld);
            skeleton->joints[pelvisIterator->second]
                .transform.translate.y += localPelvisOffset.y;
            skeleton->UpdateSkeleton();
        }
    }

    if (hasLeftSurface) {
        Vector3 leftTarget = leftFootPosition;
        leftTarget.y =
            leftHeight + ankleToGroundDistance;
        SolveLegIK(
            "upper_leg.L",
            "lower_leg.L",
            "foot.L",
            leftTarget,
            leftNormal,
            leftFootIkWeight_);

        if (showFootIKDebug_) {
            const Vector3 rayStart = {
                leftFootPosition.x,
                leftFootPosition.y + 2.0f,
                leftFootPosition.z
            };
            DebugRenderer::GetInstance()->AddLine(
                rayStart,
                leftTarget,
                { 0.15f, 0.75f, 1.0f, 1.0f },
                2.0f);
            DebugRenderer::GetInstance()->AddLine(
                leftTarget,
                leftTarget + leftNormal * 0.8f,
                { 0.15f, 1.0f, 0.30f, 1.0f },
                3.0f);
        }
    }

    if (hasRightSurface) {
        Vector3 rightTarget = rightFootPosition;
        rightTarget.y =
            rightHeight + ankleToGroundDistance;
        SolveLegIK(
            "upper_leg.R",
            "lower_leg.R",
            "foot.R",
            rightTarget,
            rightNormal,
            rightFootIkWeight_);

        if (showFootIKDebug_) {
            const Vector3 rayStart = {
                rightFootPosition.x,
                rightFootPosition.y + 2.0f,
                rightFootPosition.z
            };
            DebugRenderer::GetInstance()->AddLine(
                rayStart,
                rightTarget,
                { 1.0f, 0.35f, 0.15f, 1.0f },
                2.0f);
            DebugRenderer::GetInstance()->AddLine(
                rightTarget,
                rightTarget + rightNormal * 0.8f,
                { 0.15f, 1.0f, 0.30f, 1.0f },
                3.0f);
        }
    }

    if (hasLeftSurface || hasRightSurface) {
        playerActor_->GetObject()->Update();
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

void TestScene1::UpdateSandGolemSkeletonPose()
{
    if (!isSandGolemMode_ ||
        sandstormGolemEffectHandle_ == kInvalidEffectHandle) {
        return;
    }

    const std::array<const char*, kEffectSkeletonJointCount> jointNames = {
        "root",
        "pelvis",
        "spine",
        "chest",
        "neck",
        "head",
        "upper_arm.L",
        "forearm.L",
        "hand.L",
        "upper_arm.R",
        "forearm.R",
        "hand.R",
        "upper_leg.L",
        "lower_leg.L",
        "foot.L",
        "upper_leg.R",
        "lower_leg.R",
        "foot.R",
    };

    EffectSkeletonPose skeletonPose {};
    for (size_t jointIndex = 0;
         jointIndex < jointNames.size();
         ++jointIndex) {
        Vector3 jointWorldPosition {};
        if (!TryGetJointWorldPosition(
                jointNames[jointIndex],
                jointWorldPosition)) {
            return;
        }

        skeletonPose.jointPositions[jointIndex] = {
            jointWorldPosition.x - playerPos_.x,
            jointWorldPosition.y - playerPos_.y,
            jointWorldPosition.z - playerPos_.z,
            1.0f
        };
    }
    skeletonPose.jointCount =
        static_cast<uint32_t>(jointNames.size());

    if (!EffectManager::GetInstance()->SetEffectSkeletonPose(
            sandstormGolemEffectHandle_,
            skeletonPose)) {
        sandstormGolemEffectHandle_ = kInvalidEffectHandle;
    }
}

void TestScene1::ApplySelectedPostEffect()
{
    SceneManager* sceneManager = SceneManager::GetInstance();
    sceneManager->ClearPostEffects();
    sceneManager->AddPostEffect(
        postEffectTypes_[selectedPostEffectIndex_],
        PostEffectStage::BeforeParticle);
    sceneManager->AddPostEffect(
        PostEffectType::BlackHoleDistortion,
        PostEffectStage::BeforeParticle);
    sceneManager->SetBlackHoleRadius(0.16f);
    sceneManager->SetBlackHoleStrength(1.0f);
}

void TestScene1::UpdateBoneNames()
{
    if (!showSkeletonDebug_) {
        return;
    }

    Skeleton* skeleton = playerActor_ ? playerActor_->GetSkeleton() : nullptr;
    if (!skeleton || jointNameTexts_.size() < skeleton->joints.size()) {
        return;
    }

    for (size_t i = 0; i < skeleton->joints.size(); ++i) {
        const Joint& joint = skeleton->joints[i];
        Vector3 worldPos {};
        if (TryGetJointWorldPosition(joint.name, worldPos)) {
            // 3D 座標を 2D スクリーン座標に変換
            Vector2 screenPos = camera_->WorldToScreen(worldPos);
            
            // テキストの位置を設定して更新
            jointNameTexts_[i]->SetPosition(screenPos);
            jointNameTexts_[i]->Update();
        }
    }
}

#pragma once
#include "BaseScene.h"
#include "SceneManager.h"
#include "Engine/Camera/Camera.h"
#include "Engine/Effect/EffectManager.h"
#include "Engine/debugcamera/DebugCameraController.h"
#include "Engine/3D/Object3d.h"
#include "Engine/Animation/AnimationActor.h"
#include "Engine/Animation/Animation.h"
#include "Engine/Math/MathStruct.h"
#include "Engine/2D/Text/Text.h"
#include <array>
#include <cstddef>
#include <memory>
#include <vector>

class TestScene1 : public BaseScene {
public:
    void Initialize() override;
    void Finalize() override;
    void Update() override;
    void Draw2D() override;
    void Draw3D() override;
    void DrawParticle() override;
    void DrawImGui() override;

private:
    void ApplySelectedPostEffect();
    void UpdateSandGolemSkeletonPose();
    void ApplyFootIK();
    void SolveLegIK(
        const std::string& upperLegName,
        const std::string& lowerLegName,
        const std::string& footName,
        const Vector3& worldTarget,
        const Vector3& worldNormal,
        float ikWeight);
    bool SampleTerrainSurface(
        float worldX,
        float worldZ,
        float& worldHeight,
        Vector3& worldNormal) const;
    bool SampleFootSoleSurface(
        const Vector3& footJointPosition,
        float& worldHeight,
        Vector3& worldNormal) const;
    void UpdateKatanaAttachment();
    void ProcessAnimationEvents();
    void UpdateMovementEffects();
    void PlayDashStartBurst();
    void StopMovementEffects();
    bool TryGetJointWorldPosition(
        const std::string& jointName,
        Vector3& worldPosition) const;

    enum class PlayerAnimState {
        Idle,
        CombatIdle,
        Running,
        Dashing,
        Jumping,
        Attacking
    };

    std::unique_ptr<Camera> camera_;
    std::unique_ptr<DebugCameraController> debugCameraController_;
    std::unique_ptr<Object3d> floorObj_;
    std::unique_ptr<Object3d> ikTerrainObj_;
    Model* ikTerrainModel_ = nullptr;
    Vector3 ikTerrainPosition_ = { 0.0f, -5.0f, 0.0f };
    Vector3 ikTerrainScale_ = { 1.0f, 0.35f, 1.0f };
    static constexpr size_t kIkTestBlockCount = 7;
    std::array<std::unique_ptr<Object3d>, kIkTestBlockCount>
        ikTestBlockObjs_;
    std::array<Vector3, kIkTestBlockCount> ikTestBlockPositions_ = {
        Vector3 { -0.55f, -4.95f, 0.0f },
        Vector3 { 0.55f, -4.88f, 0.0f },
        Vector3 { -1.65f, -4.91f, 0.0f },
        Vector3 { 1.65f, -4.84f, 0.0f },
        Vector3 { -1.15f, -4.90f, 2.5f },
        Vector3 { 0.0f, -4.86f, 2.5f },
        Vector3 { 1.15f, -4.82f, 2.5f }
    };
    std::array<Vector3, kIkTestBlockCount> ikTestBlockScales_ = {
        Vector3 { 0.90f, 0.18f, 2.20f },
        Vector3 { 0.90f, 0.30f, 2.20f },
        Vector3 { 0.90f, 0.24f, 2.20f },
        Vector3 { 0.90f, 0.38f, 2.20f },
        Vector3 { 1.00f, 0.20f, 2.00f },
        Vector3 { 1.00f, 0.24f, 2.00f },
        Vector3 { 1.00f, 0.28f, 2.00f }
    };
    std::array<Vector3, kIkTestBlockCount> ikTestBlockRotations_ = {
        Vector3 { 0.0f, 0.0f, 0.0f },
        Vector3 { 0.0f, 0.0f, 0.0f },
        Vector3 { 0.0f, 0.0f, 0.0f },
        Vector3 { 0.0f, 0.0f, 0.0f },
        Vector3 { 0.0f, 0.0f, -0.10f },
        Vector3 { 0.12f, 0.0f, 0.08f },
        Vector3 { -0.10f, 0.0f, 0.12f }
    };
    std::unique_ptr<Object3d> katanaObj_;
    std::unique_ptr<Object3d> recoveryCubeObj_;
    Vector3 recoveryCubeBasePosition_ = { -6.0f, -3.5f, -4.0f };
    float recoveryCubeAnimationTime_ = 0.0f;
    EffectHandle recoveryEffectHandle_ = kInvalidEffectHandle;

    // Player (Robo)
    std::unique_ptr<AnimationActor> playerActor_;
    std::unique_ptr<AnimationActor> sneakWalkActor_;
    Animation runAnimation_;
    Animation dashAnimation_;
    Animation jumpAnimation_;
    Animation idleAnimation_;
    Animation combatIdleAnimation_;
    Animation attackAnimation_;
    Animation leftPunchAnimation_;
    Animation rocketUppercutAnimation_;
    Vector3 playerPos_ = { 0.0f, -5.0f, 0.0f };
    Vector3 playerRot_ = { 0.0f, 0.0f, 0.0f };
    float playerScale_ = 0.4f;
    float playerRotOffset_ = 0.0f;
    Vector3 katanaGripPosition_ = { -1.3f, 0.7f, -2.2f };
    Vector3 katanaOffset_ = { 0.0f, 0.0f, 0.0f };
    Vector3 katanaRotation_ = { 3.14159265f, 0.0f, 0.0f };
    float katanaScale_ = 0.65f;
    PlayerAnimState currentAnimState_ = PlayerAnimState::Idle;
    float idleVariationTimer_ = 0.0f;
    float combatIdleTimer_ = 0.0f;

    EffectHandle fieldDemoEffectHandle_ = kInvalidEffectHandle;
    EffectHandle cyberSingularityEffectHandle_ = kInvalidEffectHandle;
    EffectHandle cyberSingularityDebrisHandle_ = kInvalidEffectHandle;
    EffectHandle cyberSingularityJetsHandle_ = kInvalidEffectHandle;
    EffectHandle sandstormGolemEffectHandle_ = kInvalidEffectHandle;
    bool isSandGolemMode_ = false;
    EffectHandle bodySpeedLineEffectHandle_ = kInvalidEffectHandle;
    EffectHandle backflipTrailEffectHandle_ = kInvalidEffectHandle;
    EffectHandle groundLightningOuterHandle_ = kInvalidEffectHandle;
    EffectHandle groundLightningInnerHandle_ = kInvalidEffectHandle;
    EffectHandle groundLightningMotesHandle_ = kInvalidEffectHandle;
    bool showFieldDebug_ = false;
    bool showSkeletonDebug_ = true;
    bool enableFootIK_ = true;
    bool showFootIKDebug_ = true;
    float leftFootIkWeight_ = 0.0f;
    float rightFootIkWeight_ = 0.0f;
    float footSoleGroundMargin_ = 0.08f;

    // TPS Camera
    float cameraYaw_ = 0.0f;
    float cameraPitch_ = 0.2f;
    float cameraDistance_ = 18.0f;

    // Jump Control
    bool isJumping_ = false;
    float jumpVelocity_ = 0.0f;
    const float gravity_ = 0.015f;

    // Attack Control
    float attackTimer_ = 0.0f;
    int comboStep_ = 0;
    int queuedComboAttacks_ = 0;
    std::string lastAnimationEventName_;

    std::array<PostEffectType, 34> postEffectTypes_ = {
        PostEffectType::Copy,
        PostEffectType::GrayScale,
        PostEffectType::Vignette,
        PostEffectType::DepthOfField,
        PostEffectType::MotionBlur,
        PostEffectType::ChromaticAberration,
        PostEffectType::LensDistortion,
        PostEffectType::FilmGrain,
        PostEffectType::LensDirt,
        PostEffectType::CameraShake,
        PostEffectType::BokehShape,
        PostEffectType::Fisheye,
        PostEffectType::Pixelate,
        PostEffectType::ColorAdjust,
        PostEffectType::smoothing,
        PostEffectType::GaussianFilter,
        PostEffectType::LuminanceBasedOutline,
        PostEffectType::DepthOutline,
        PostEffectType::RadialBlur,
        PostEffectType::Dissolve,
        PostEffectType::Random,
        PostEffectType::Bloom,
        PostEffectType::LensFlare,
        PostEffectType::Glare,
        PostEffectType::LightShafts,
        PostEffectType::VolumetricLight,
        PostEffectType::AnamorphicFlare,
        PostEffectType::Halo,
        PostEffectType::LightStreak,
        PostEffectType::NeonGlow,
        PostEffectType::GhostImage,
        PostEffectType::Outline,
        PostEffectType::Fog,
        PostEffectType::FocusLine,
    };
    std::size_t selectedPostEffectIndex_ = 0;

    // Snow Effect
    EffectHandle snowEffectHandle_ = kInvalidEffectHandle;

    // Hand Joint Flame Tracking Effects
    EffectHandle leftHandFlameHandle_ = kInvalidEffectHandle;
    EffectHandle rightHandFlameHandle_ = kInvalidEffectHandle;

    // Hinokami Kagura Effects
    EffectHandle hinokamiFlameHandle_ = kInvalidEffectHandle;
    EffectHandle hinokamiEmbersHandle_ = kInvalidEffectHandle;

    // Delayed 6-Direction Firework Launch & Double Burst Sequence Triggers
    float sixTrailTimer_ = 0.0f;
    bool isSixTrailPending_ = false;
    float sixBlastTimer_ = 0.0f;
    bool isSixBlastPending_ = false;

    // 二重破裂（2段階目爆発）用タイマー
    float secondBlastTimer_ = 0.0f;
    bool isSecondBlastPending_ = false;

    // 一連の演出全体のクールダウン・連打防止ガード
    float coolDownTimer_ = 0.0f;
    bool isSequenceActive_ = false;

    Vector3 sixSeqCenterPos_{};
    std::vector<std::unique_ptr<Text>> jointNameTexts_;
    void UpdateBoneNames();
};

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
#include <array>
#include <cstddef>
#include <memory>

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
    void UpdateKatanaAttachment();
    void ProcessAnimationEvents();
    void UpdateMovementEffects();
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
    std::unique_ptr<Object3d> katanaObj_;

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
    EffectHandle bodySpeedLineEffectHandle_ = kInvalidEffectHandle;
    EffectHandle backflipTrailEffectHandle_ = kInvalidEffectHandle;
    bool showFieldDebug_ = true;

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
};

#pragma once
#include "BaseScene.h"
#include "SceneManager.h"
#include "Engine/Camera/Camera.h"
#include "Engine/debugcamera/DebugCameraController.h"
#include "Engine/3D/Object3d.h"
#include "Engine/Animation/AnimationActor.h"
#include "Engine/Animation/Animation.h"
#include "Engine/Math/MathStruct.h"
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
    enum class PlayerAnimState {
        Idle,
        Running,
        Jumping,
        Attacking
    };

    std::unique_ptr<Camera> camera_;
    std::unique_ptr<DebugCameraController> debugCameraController_;
    std::unique_ptr<Object3d> floorObj_;

    // Player (Robo)
    std::unique_ptr<AnimationActor> playerActor_;
    std::unique_ptr<AnimationActor> sneakWalkActor_;
    Animation runAnimation_;
    Animation jumpAnimation_;
    Animation idleAnimation_;
    Animation attackAnimation_;
    Vector3 playerPos_ = { 0.0f, -5.0f, 0.0f };
    Vector3 playerRot_ = { 0.0f, 0.0f, 0.0f };
    float playerScale_ = 3.0f;
    float playerRotOffset_ = -1.57079f;
    PlayerAnimState currentAnimState_ = PlayerAnimState::Idle;

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
    bool hasEmittedParticle_ = false;
};

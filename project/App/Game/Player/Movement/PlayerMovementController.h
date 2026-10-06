#pragma once
#include "Engine/math/MathStruct.h"
#include "App/Game/Player/Aim/PlayerAimController.h"
#include "PlayerScreenConstraint.h"
#include "PlayerRollController.h"
#include "PlayerSteeringController.h"

class Camera;
class Input;

class PlayerMovementController {
public:
    enum class ControlMode { KeyboardAndMouse, StarFox };
    PlayerMovementController(EulerTransform& transform, const PlayerAimController& aimController)
        : transform_(transform), aimController_(aimController) {}
    PlayerMovementController(const PlayerMovementController&) = delete;
    PlayerMovementController& operator=(const PlayerMovementController&) = delete;
    PlayerMovementController(PlayerMovementController&&) = delete;
    PlayerMovementController& operator=(PlayerMovementController&&) = delete;
    void Initialize();
    void SetCamera(Camera* camera) { screenConstraint_.SetCamera(camera); }
    void UpdateBoost(Input* input, bool isDebugMode);
    void UpdateMovement(Input* input);
    void ApplyRailPosition();
    void Stop();
    void EnableAllRangeMode(float areaRadius, float minHeight, float maxHeight);
    bool IsAllRangeMode() const { return isAllRangeMode_; }
    bool IsReturningToFlightArea() const { return isReturningToFlightArea_; }
    bool IsBoosting() const { return isBoosting_; }
    bool IsRolling() const { return rollController_.IsRolling(); }
    const Vector3& GetFlightForward() const { return railForward_; }
    const Vector3& GetRailOffset() const { return railOffset_; }
    Vector3 GetWorldVelocity() const { return railForward_ * velocity_.z; }
    Vector3 GetAutomaticWorldVelocity() const;
    void SetRailFrame(const Vector3& railBasePosition, const Vector3& railRight,
        const Vector3& railUp, const Vector3& railForward);
    void ApplyRailAreaForce(const Vector3& force);
    void SetControlMode(ControlMode mode);
    ControlMode GetControlMode() const { return controlMode_; }
    void SetMouseSensitivity(float sensitivity) { steeringController_.SetMouseSensitivity(sensitivity); }
    float GetMouseSensitivity() const { return steeringController_.GetMouseSensitivity(); }
    const Vector2& GetStarFoxSteeringInput() const { return steeringController_.GetSteeringInput(); }

private:
    void UpdateKeyboardMove(Input* input);
    void UpdateStarFoxMove();
    void UpdateStarFoxSteering();
    void UpdateAllRangeMove(Input* input);
    void UpdateFlightOrientation(const Vector2& steering, float deltaTimeSeconds);
    void UpdateFlightAreaReturn(Vector2& steering, float deltaTimeSeconds);
    void UpdateFlightHeight(float deltaTimeSeconds);
    void UpdateFlightPosition(float deltaTimeSeconds);
    void UpdateFlightBank(const Vector2& steering, float deltaTimeSeconds);

    EulerTransform& transform_;
    const PlayerAimController& aimController_;
    bool isBoosting_ = false;
    ControlMode controlMode_ = ControlMode::StarFox;
    bool isAllRangeMode_ = false;
    bool isReturningToFlightArea_ = false;
    float flightAreaRadius_ = 600.0f;
    float flightMinHeight_ = 5.0f;
    float flightMaxHeight_ = 180.0f;
    float flightYaw_ = 0.0f;
    float flightPitch_ = 0.0f;
    float normalMaxSpeed_ = 0.5f;
    float boostMaxSpeed_ = 1.0f;
    float normalAcceleration_ = 0.2f;
    float boostAcceleration_ = 0.35f;
    float moveSpeed_ = normalAcceleration_;
    Vector3 velocity_ = { 0.0f, 0.0f, 0.5f };
    Vector3 railBasePosition_ = { 0.0f, 0.0f, 0.0f };
    Vector3 railRight_ = { 1.0f, 0.0f, 0.0f };
    Vector3 railUp_ = { 0.0f, 1.0f, 0.0f };
    Vector3 railForward_ = { 0.0f, 0.0f, 1.0f };
    Vector3 railOffset_ = { 0.0f, 0.0f, 0.0f };
    PlayerScreenConstraint screenConstraint_{ railBasePosition_, railRight_, railUp_, railForward_ };
    PlayerRollController rollController_;
    PlayerSteeringController steeringController_;
};

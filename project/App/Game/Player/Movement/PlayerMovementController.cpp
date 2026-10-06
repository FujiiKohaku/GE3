#include "App/Game/Player/Movement/PlayerMovementController.h"
#include "Engine/Input/Input.h"
#include "Engine/Time/TimeManager.h"
#include "Engine/audio/SoundManager.h"
#include "Engine/Winapp/WinApp.h"
#include <algorithm>
#include <cmath>
#include <numbers>

void PlayerMovementController::Initialize()
{
    railBasePosition_ = transform_.translate;
    railOffset_ = {};
}

void PlayerMovementController::UpdateMovement(Input* input)
{
    rollController_.Update(transform_, input->IsKeyTrigger(DIK_A), input->IsKeyTrigger(DIK_D));
    if (isAllRangeMode_) {
        UpdateAllRangeMove(input);
    } else if (controlMode_ == ControlMode::StarFox) {
        UpdateStarFoxMove();
    } else {
        UpdateKeyboardMove(input);
    }
}

void PlayerMovementController::ApplyRailPosition()
{
    if (!isAllRangeMode_) {
        transform_.translate = screenConstraint_.CalculateRailWorldPosition(railOffset_);
    }
}

void PlayerMovementController::Stop()
{
    isBoosting_ = false;
    rollController_.Stop();
}

Vector3 PlayerMovementController::GetAutomaticWorldVelocity() const
{
    if (isAllRangeMode_) {
        return railForward_ * velocity_.z;
    }
    return railForward_ * normalMaxSpeed_;
}

void PlayerMovementController::SetRailFrame(const Vector3& railBasePosition,
    const Vector3& railRight, const Vector3& railUp, const Vector3& railForward)
{
    railBasePosition_ = railBasePosition;
    railRight_ = railRight;
    railUp_ = railUp;
    railForward_ = railForward;
}

void PlayerMovementController::ApplyRailAreaForce(const Vector3& force)
{
    railOffset_.x += force.x;
    railOffset_.y += force.y;
    railOffset_ = screenConstraint_.ClampRailOffsetToScreen(railOffset_);
}

void PlayerMovementController::SetControlMode(ControlMode mode)
{
    if (controlMode_ != mode) {
        steeringController_.Reset();
    }
    controlMode_ = mode;
}
void PlayerMovementController::UpdateBoost(Input* input, bool isDebugMode)
{
    const bool wasBoosting = isBoosting_;
    isBoosting_ = !isDebugMode &&
        (input->IsKeyPressed(DIK_LSHIFT) || input->IsMousePressed(1));
    if (isBoosting_ && !wasBoosting) {
        SoundManager::GetInstance()->Play("BoostStart");
    }
    velocity_.z = normalMaxSpeed_;
    moveSpeed_ = normalAcceleration_;
    if (isBoosting_) {
        velocity_.z = boostMaxSpeed_;
        moveSpeed_ = boostAcceleration_;
    }
}

void PlayerMovementController::UpdateKeyboardMove(Input* input)
{

    Vector3 nextRailOffset = railOffset_;

    if (input->IsKeyPressed(DIK_A)) {
        nextRailOffset.x -= moveSpeed_;
    }

    if (input->IsKeyPressed(DIK_D)) {
        nextRailOffset.x += moveSpeed_;
    }

    if (input->IsKeyPressed(DIK_W)) {
        nextRailOffset.y += moveSpeed_;
    }

    if (input->IsKeyPressed(DIK_S)) {
        nextRailOffset.y -= moveSpeed_;
    }

    railOffset_ = screenConstraint_.ClampRailOffsetToScreen(nextRailOffset);
}

void PlayerMovementController::UpdateStarFoxMove()
{
    UpdateStarFoxSteering();
    constexpr float kStarFoxResponse = 2.75f;
    Vector3 nextRailOffset = railOffset_;
    nextRailOffset.x +=
        steeringController_.GetSteeringInput().x * moveSpeed_ * kStarFoxResponse;
    nextRailOffset.y +=
        steeringController_.GetSteeringInput().y * moveSpeed_ * kStarFoxResponse;
    railOffset_ = screenConstraint_.ClampRailOffsetToScreen(nextRailOffset);
}

void PlayerMovementController::EnableAllRangeMode(float areaRadius, float minHeight, float maxHeight)
{
    isAllRangeMode_ = true;
    flightAreaRadius_ = areaRadius;
    flightMinHeight_ = minHeight;
    flightMaxHeight_ = maxHeight;
    flightYaw_ = -transform_.rotate.y;
    flightPitch_ = -transform_.rotate.x;
    railForward_ = {
        std::sin(flightYaw_) * std::cos(flightPitch_),
        std::sin(flightPitch_),
        std::cos(flightYaw_) * std::cos(flightPitch_)
    };
    railRight_ = Normalize(Cross(Vector3 { 0.0f, 1.0f, 0.0f }, railForward_));
    railUp_ = Normalize(Cross(railForward_, railRight_));
    isReturningToFlightArea_ = false;
    railOffset_ = {};
    steeringController_.Reset();
}

void PlayerMovementController::UpdateAllRangeMove(Input* input)
{
    UpdateStarFoxSteering();
    Vector2 steering = steeringController_.GetSteeringInput();
    if (controlMode_ == ControlMode::KeyboardAndMouse) {
        steering = {};
        if (input->IsKeyPressed(DIK_A)) steering.x -= 1.0f;
        if (input->IsKeyPressed(DIK_D)) steering.x += 1.0f;
        if (input->IsKeyPressed(DIK_W)) steering.y += 1.0f;
        if (input->IsKeyPressed(DIK_S)) steering.y -= 1.0f;
    }

    const float deltaTimeSeconds = TimeManager::GetInstance()->GetDeltaTime();
    UpdateFlightOrientation(steering, deltaTimeSeconds);
    UpdateFlightAreaReturn(steering, deltaTimeSeconds);
    UpdateFlightHeight(deltaTimeSeconds);
    UpdateFlightPosition(deltaTimeSeconds);
    UpdateFlightBank(steering, deltaTimeSeconds);
}

void PlayerMovementController::UpdateFlightOrientation(const Vector2& steering, float deltaTimeSeconds)
{
    constexpr float kYawSpeed = 1.35f;
    constexpr float kPitchSpeed = 0.85f;
    constexpr float kMaxPitch = 0.95f;
    flightYaw_ += steering.x * kYawSpeed * deltaTimeSeconds;
    flightPitch_ = std::clamp(
        flightPitch_ + steering.y * kPitchSpeed * deltaTimeSeconds, -kMaxPitch, kMaxPitch);
}

void PlayerMovementController::UpdateFlightAreaReturn(Vector2& steering, float deltaTimeSeconds)
{
    constexpr float kYawSpeed = 1.35f;
    // Begin an automatic inward turn early enough to keep the chase camera inside the ocean.
    const float horizontalDistance = std::sqrt(
        transform_.translate.x * transform_.translate.x +
        transform_.translate.z * transform_.translate.z);
    if (horizontalDistance > flightAreaRadius_ - 90.0f) {
        isReturningToFlightArea_ = true;
    } else if (horizontalDistance < flightAreaRadius_ - 150.0f) {
        isReturningToFlightArea_ = false;
    }
    if (isReturningToFlightArea_) {
        flightYaw_ -= steering.x * kYawSpeed * deltaTimeSeconds;
        const float inwardYaw = std::atan2(-transform_.translate.x, -transform_.translate.z);
        const float yawDifference = std::remainder(
            inwardYaw - flightYaw_, 2.0f * std::numbers::pi_v<float>);
        flightYaw_ += std::clamp(yawDifference, -2.0f * deltaTimeSeconds, 2.0f * deltaTimeSeconds);
        steering.x = std::clamp(yawDifference, -1.0f, 1.0f);
    }
    flightYaw_ = std::remainder(flightYaw_, 2.0f * std::numbers::pi_v<float>);
}

void PlayerMovementController::UpdateFlightHeight(float deltaTimeSeconds)
{
    if (transform_.translate.y < flightMinHeight_ + 12.0f && flightPitch_ < 0.0f) {
        flightPitch_ += (0.25f - flightPitch_) * (1.0f - std::exp(-4.0f * deltaTimeSeconds));
    }
    if (transform_.translate.y > flightMaxHeight_ - 12.0f && flightPitch_ > 0.0f) {
        flightPitch_ += (-0.25f - flightPitch_) * (1.0f - std::exp(-4.0f * deltaTimeSeconds));
    }
}

void PlayerMovementController::UpdateFlightPosition(float deltaTimeSeconds)
{
    railForward_ = {
        std::sin(flightYaw_) * std::cos(flightPitch_),
        std::sin(flightPitch_),
        std::cos(flightYaw_) * std::cos(flightPitch_)
    };
    railRight_ = Normalize(Cross(Vector3 { 0.0f, 1.0f, 0.0f }, railForward_));
    railUp_ = Normalize(Cross(railForward_, railRight_));
    transform_.translate += railForward_ * (velocity_.z * 60.0f * deltaTimeSeconds);
    const float distanceAfterMove = std::sqrt(
        transform_.translate.x * transform_.translate.x +
        transform_.translate.z * transform_.translate.z);
    if (distanceAfterMove > flightAreaRadius_) {
        const float correction = flightAreaRadius_ / distanceAfterMove;
        transform_.translate.x *= correction;
        transform_.translate.z *= correction;
    }
    transform_.translate.y = std::clamp(
        transform_.translate.y, flightMinHeight_, flightMaxHeight_);
    railBasePosition_ = transform_.translate;
    transform_.rotate.x = -flightPitch_;
    transform_.rotate.y = -flightYaw_;
}

void PlayerMovementController::UpdateFlightBank(const Vector2& steering, float deltaTimeSeconds)
{
    if (!rollController_.IsRolling()) {
        const float targetBank = -steering.x * 0.65f;
        transform_.rotate.z += (targetBank - transform_.rotate.z) *
            (1.0f - std::exp(-7.0f * deltaTimeSeconds));
    }
}

void PlayerMovementController::UpdateStarFoxSteering()
{
    const Vector2 screenSizePixels = {
        static_cast<float>(WinApp::GetInstance()->GetClientWidth()),
        static_cast<float>(WinApp::GetInstance()->GetClientHeight())
    };
    steeringController_.Update(aimController_.GetAimScreenPosition(), screenSizePixels,
        isAllRangeMode_, TimeManager::GetInstance()->GetDeltaTime());
}

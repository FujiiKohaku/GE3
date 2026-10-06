#include "PlayerSteeringController.h"
#include <algorithm>
#include <cmath>

namespace {
float ApplySteeringDeadZone(float value)
{
    constexpr float kDeadZone = 0.05f;
    const float magnitude = std::abs(value);
    if (magnitude <= kDeadZone) {
        return 0.0f;
    }
    return std::copysign((magnitude - kDeadZone) / (1.0f - kDeadZone), value);
}
}

void PlayerSteeringController::SetMouseSensitivity(float sensitivity)
{
    mouseSensitivity_ = std::clamp(sensitivity, 0.5f, 2.0f);
}

void PlayerSteeringController::Update(const Vector2& aimPosition, const Vector2& screenSizePixels,
    bool isAllRangeMode, float deltaTimeSeconds)
{
    const float screenWidth = screenSizePixels.x;
    const float screenHeight = screenSizePixels.y;
    if (screenWidth <= 0.0f || screenHeight <= 0.0f) {
        return;
    }

    // The cursor behaves like an analog stick: the center is neutral and the
    // ship moves faster as the cursor gets farther from the center.
    float inputX = (aimPosition.x - screenWidth * 0.5f) /
        (screenWidth * 0.5f);
    float inputY = (screenHeight * 0.5f - aimPosition.y) /
        (screenHeight * 0.5f);

    inputX = ApplySteeringDeadZone(std::clamp(
        inputX * mouseSensitivity_, -1.0f, 1.0f));
    inputY = ApplySteeringDeadZone(std::clamp(
        inputY * mouseSensitivity_, -1.0f, 1.0f));

    float steeringLerpRate = 0.20f;
    if (isAllRangeMode) {
        steeringLerpRate = 1.0f - std::exp(
            -13.4f * deltaTimeSeconds);
    }
    steeringInput_.x +=
        (inputX - steeringInput_.x) * steeringLerpRate;
    steeringInput_.y +=
        (inputY - steeringInput_.y) * steeringLerpRate;

}

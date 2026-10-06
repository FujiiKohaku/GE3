#include "PlayerScreenConstraint.h"
#include "Engine/Camera/Camera.h"
#include "Engine/Winapp/WinApp.h"
#include <algorithm>
#include <cmath>

Vector2 PlayerScreenConstraint::CalculateScreenCorrection(const Vector3& railOffset) const
{
    Vector2 correction {};
    correction.x = 0.0f;
    correction.y = 0.0f;

    if (camera_ == nullptr) {
        return correction;
    }

    Vector3 playerPosition = CalculateRailWorldPosition(railOffset);
    Vector2 screenPosition = camera_->WorldToScreen(playerPosition);

    float minX = screenPosition.x;
    float maxX = screenPosition.x;
    float minY = screenPosition.y;
    float maxY = screenPosition.y;

    Vector3 rightExtent = right_ * playerBoundsHalfWidth_;
    Vector3 upExtent = up_ * playerBoundsHalfHeight_;

    UpdateScreenBounds(playerPosition + rightExtent + upExtent, minX, maxX, minY, maxY);
    UpdateScreenBounds(playerPosition + rightExtent - upExtent, minX, maxX, minY, maxY);
    UpdateScreenBounds(playerPosition - rightExtent + upExtent, minX, maxX, minY, maxY);
    UpdateScreenBounds(playerPosition - rightExtent - upExtent, minX, maxX, minY, maxY);

    float leftLimit = playerClampMarginX_;
    float rightLimit = static_cast<float>(WinApp::GetInstance()->GetClientWidth()) - playerClampMarginX_;

    float topLimit = playerClampMarginY_;
    float bottomLimit = static_cast<float>(WinApp::GetInstance()->GetClientHeight()) - playerClampMarginY_;

    if (minX < leftLimit) {
        correction.x = leftLimit - minX;
    } else if (maxX > rightLimit) {
        correction.x = rightLimit - maxX;
    }

    if (minY < topLimit) {
        correction.y = topLimit - minY;
    } else if (maxY > bottomLimit) {
        correction.y = bottomLimit - maxY;
    }

    return correction;
}

Vector3 PlayerScreenConstraint::CalculateRailWorldPosition(const Vector3& railOffset) const
{
    return basePosition_ + right_ * railOffset.x + up_ * railOffset.y + forward_ * railOffset.z;
}

Vector3 PlayerScreenConstraint::ClampRailOffsetToScreen(const Vector3& railOffset) const
{
    Vector3 correctedRailOffset = railOffset;
    correctedRailOffset.x = std::clamp(
        correctedRailOffset.x, -railMoveLimitX_, railMoveLimitX_);
    correctedRailOffset.y = std::clamp(
        correctedRailOffset.y, -railMoveLimitY_, railMoveLimitY_);
    correctedRailOffset.z = 0.0f;

    if (camera_ == nullptr) {
        return correctedRailOffset;
    }

    constexpr int kCorrectionCount = 3;
    for (int correctionIndex = 0; correctionIndex < kCorrectionCount; ++correctionIndex) {
        Vector2 screenCorrection = CalculateScreenCorrection(correctedRailOffset);

        if (std::fabs(screenCorrection.x) < 0.01f && std::fabs(screenCorrection.y) < 0.01f) {
            break;
        }

        Vector2 baseScreen = camera_->WorldToScreen(CalculateRailWorldPosition(correctedRailOffset));

        Vector3 rightOffset = correctedRailOffset;
        rightOffset.x += 1.0f;
        Vector2 rightScreen = camera_->WorldToScreen(CalculateRailWorldPosition(rightOffset));

        Vector3 upOffset = correctedRailOffset;
        upOffset.y += 1.0f;
        Vector2 upScreen = camera_->WorldToScreen(CalculateRailWorldPosition(upOffset));

        float rightScreenX = rightScreen.x - baseScreen.x;
        float rightScreenY = rightScreen.y - baseScreen.y;
        float upScreenX = upScreen.x - baseScreen.x;
        float upScreenY = upScreen.y - baseScreen.y;

        float determinant = rightScreenX * upScreenY - rightScreenY * upScreenX;

        if (std::fabs(determinant) > 0.0001f) {
            float offsetX = (screenCorrection.x * upScreenY - screenCorrection.y * upScreenX) / determinant;
            float offsetY = (rightScreenX * screenCorrection.y - rightScreenY * screenCorrection.x) / determinant;

            correctedRailOffset.x += offsetX;
            correctedRailOffset.y += offsetY;
        } else {
            float rightLengthSquared = rightScreenX * rightScreenX + rightScreenY * rightScreenY;
            if (rightLengthSquared > 0.0001f) {
                float offsetX = (screenCorrection.x * rightScreenX + screenCorrection.y * rightScreenY) / rightLengthSquared;
                correctedRailOffset.x += offsetX;
            }

            float upLengthSquared = upScreenX * upScreenX + upScreenY * upScreenY;
            if (upLengthSquared > 0.0001f) {
                float offsetY = (screenCorrection.x * upScreenX + screenCorrection.y * upScreenY) / upLengthSquared;
                correctedRailOffset.y += offsetY;
            }
        }

        correctedRailOffset.x = std::clamp(
            correctedRailOffset.x, -railMoveLimitX_, railMoveLimitX_);
        correctedRailOffset.y = std::clamp(
            correctedRailOffset.y, -railMoveLimitY_, railMoveLimitY_);
        correctedRailOffset.z = 0.0f;
    }

    return correctedRailOffset;
}

void PlayerScreenConstraint::UpdateScreenBounds(const Vector3& worldPosition, float& minX, float& maxX, float& minY, float& maxY) const
{
    if (camera_ == nullptr) {
        return;
    }

    Vector2 screenPosition = camera_->WorldToScreen(worldPosition);
    minX = (std::min)(minX, screenPosition.x);
    maxX = (std::max)(maxX, screenPosition.x);
    minY = (std::min)(minY, screenPosition.y);
    maxY = (std::max)(maxY, screenPosition.y);
}


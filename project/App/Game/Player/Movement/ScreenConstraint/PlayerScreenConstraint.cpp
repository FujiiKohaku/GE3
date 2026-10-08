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

    // 機体の中心だけでなく、レールの左右・上下に広がる矩形の4隅も投影する。
    Vector3 rightExtent = right_ * playerBoundsHalfWidth_;
    Vector3 upExtent = up_ * playerBoundsHalfHeight_;

    UpdateScreenBounds(playerPosition + rightExtent + upExtent, minX, maxX, minY, maxY);
    UpdateScreenBounds(playerPosition + rightExtent - upExtent, minX, maxX, minY, maxY);
    UpdateScreenBounds(playerPosition - rightExtent + upExtent, minX, maxX, minY, maxY);
    UpdateScreenBounds(playerPosition - rightExtent - upExtent, minX, maxX, minY, maxY);

    // 画面端の余白を除いた範囲を、機体を収める領域とする。
    float leftLimit = playerClampMarginX_;
    float rightLimit = static_cast<float>(WinApp::GetInstance()->GetClientWidth()) - playerClampMarginX_;

    float topLimit = playerClampMarginY_;
    float bottomLimit = static_cast<float>(WinApp::GetInstance()->GetClientHeight()) - playerClampMarginY_;

    // はみ出した側を画面内へ戻す量を求める。まだワールド座標の移動量には変換しない。
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
    // 基準位置に、レールの左右・上下・前後方向の移動量を足す。
    return basePosition_ + right_ * railOffset.x + up_ * railOffset.y + forward_ * railOffset.z;
}

Vector3 PlayerScreenConstraint::ClampRailOffsetToScreen(const Vector3& railOffset) const
{
    Vector3 correctedRailOffset = railOffset;
    // 先にレール上の移動範囲へ制限する。前後方向のオフセットは使わない。
    correctedRailOffset.x = std::clamp(
        correctedRailOffset.x, -railMoveLimitX_, railMoveLimitX_);
    correctedRailOffset.y = std::clamp(
        correctedRailOffset.y, -railMoveLimitY_, railMoveLimitY_);
    correctedRailOffset.z = 0.0f;

    if (camera_ == nullptr) {
        return correctedRailOffset;
    }

    // 投影による誤差を減らすため、補正を最大3回繰り返す。
    // カメラが未設定なら上の移動範囲の制限だけで終了する。
    constexpr int kCorrectionCount = 3;
    for (int correctionIndex = 0; correctionIndex < kCorrectionCount; ++correctionIndex) {
        Vector2 screenCorrection = CalculateScreenCorrection(correctedRailOffset);

        if (std::fabs(screenCorrection.x) < 0.01f && std::fabs(screenCorrection.y) < 0.01f) {
            break;
        }

        // 左右・上下へそれぞれ1単位動かした場合の、画面上の変化量を調べる。
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

        // 画面上の補正量を、レールの左右・上下の移動量へ変換するための行列式。
        float determinant = rightScreenX * upScreenY - rightScreenY * upScreenX;

        if (std::fabs(determinant) > 0.0001f) {
            float offsetX = (screenCorrection.x * upScreenY - screenCorrection.y * upScreenX) / determinant;
            float offsetY = (rightScreenX * screenCorrection.y - rightScreenY * screenCorrection.x) / determinant;

            correctedRailOffset.x += offsetX;
            correctedRailOffset.y += offsetY;
        } else {
            // 投影した2方向がほぼ重なる場合は、各方向への成分を使って補正する。
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

        // 画面内補正でレールの移動範囲を超えないよう、もう一度制限する。
        correctedRailOffset.x = std::clamp(correctedRailOffset.x, -railMoveLimitX_, railMoveLimitX_);
        correctedRailOffset.y = std::clamp(correctedRailOffset.y, -railMoveLimitY_, railMoveLimitY_);
        correctedRailOffset.z = 0.0f;
    }

    return correctedRailOffset;
}

void PlayerScreenConstraint::UpdateScreenBounds(const Vector3& worldPosition, float& minX, float& maxX, float& minY, float& maxY) const
{
    if (camera_ == nullptr) {
        return;
    }

    // ワールド座標を画面へ投影し、左右・上下の最小値と最大値を更新する。
    Vector2 screenPosition = camera_->WorldToScreen(worldPosition);
    minX = (std::min)(minX, screenPosition.x);
    maxX = (std::max)(maxX, screenPosition.x);
    minY = (std::min)(minY, screenPosition.y);
    maxY = (std::max)(maxY, screenPosition.y);
}


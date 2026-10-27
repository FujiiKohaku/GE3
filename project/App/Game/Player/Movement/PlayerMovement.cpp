#include "PlayerMovement.h"
#include "App/Game/Common/GameTiming.h"
#include "Engine/Camera/Camera.h"
#include "Engine/Winapp/WinApp.h"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace {
// 中央付近の小さな入力を無視し、意図しない操舵を防ぐ。
float ApplySteeringDeadZone(float value)
{
    constexpr float kDeadZone = 0.05f;
    const float magnitude = std::abs(value);
    if (magnitude <= kDeadZone) {
        return 0.0f;
    }
    // デッドゾーンの外側を0～1へ引き伸ばし、元の左右・上下の符号を戻す。
    return std::copysign((magnitude - kDeadZone) / (1.0f - kDeadZone), value);
}
}

void PlayerMovement::InitializeMovement(const EulerTransform& transform)
{
    railBasePosition_ = transform.translate;
    railOffset_ = {};
}

void PlayerMovement::SynchronizePosition(const EulerTransform& transform)
{
    // 現在のオフセットを維持し、次のレール位置計算でも設定位置を再現する。
    railBasePosition_ = transform.translate - railRight_ * railOffset_.x -
        railUp_ * railOffset_.y - railForward_ * railOffset_.z;
}

void PlayerMovement::SynchronizeOrientation(const EulerTransform& transform)
{
    if (!isAllRangeMode_) {
        return;
    }
    flightYaw_ = -transform.rotate.y;
    flightPitch_ = -transform.rotate.x;
    railForward_ = { std::sin(flightYaw_) * std::cos(flightPitch_),
        std::sin(flightPitch_), std::cos(flightYaw_) * std::cos(flightPitch_) };
    railRight_ = Normalize(Cross(Vector3{ 0.0f, 1.0f, 0.0f }, railForward_));
    railUp_ = Normalize(Cross(railForward_, railRight_));
}

void PlayerMovement::ApplyRailPosition(EulerTransform& transform)
{
    if (!isAllRangeMode_) {
        transform.translate = CalculateRailWorldPosition(railOffset_);
    }
}

void PlayerMovement::StopMovement()
{
    // 死亡時に操作状態を解除する。速度や位置を0にする処理ではない。
    isBoosting_ = false;
    StopRoll();
    manualBank_ = 0.0f;
}

Vector3 PlayerMovement::GetAutomaticMovementVelocity() const
{
    if (isAllRangeMode_) {
        return railForward_ * velocity_.z;
    }
    return railForward_ * normalMaxSpeed_;
}

void PlayerMovement::SetRailFrame(const Vector3& railBasePosition,
    const Vector3& railRight, const Vector3& railUp, const Vector3& railForward)
{
    railBasePosition_ = railBasePosition;
    railRight_ = railRight;
    railUp_ = railUp;
    railForward_ = railForward;
}

void PlayerMovement::ApplyRailAreaForce(const Vector3& force, float deltaTimeSeconds)
{
    const float referenceStep = GameTiming::CalculateReferenceStep(deltaTimeSeconds);
    railOffset_.x += force.x * referenceStep;
    railOffset_.y += force.y * referenceStep;
    railOffset_ = ClampRailOffsetToScreen(railOffset_);
}

bool PlayerMovement::UpdateBoost(bool isBraking, bool shouldBoost)
{
    constexpr float kBrakeSpeedMultiplier = 0.5f;
    const bool wasBoosting = isBoosting_;
    isBoosting_ = shouldBoost && !isBraking;
    velocity_.z = normalMaxSpeed_;
    moveSpeed_ = normalAcceleration_;
    if (isBraking) {
        velocity_.z = normalMaxSpeed_ * kBrakeSpeedMultiplier;
    } else if (isBoosting_) {
        velocity_.z = boostMaxSpeed_;
        moveSpeed_ = boostAcceleration_;
    }
    return isBoosting_ && !wasBoosting;
}

void PlayerMovement::EnableAllRangeMode(const EulerTransform& transform, float areaRadius, float minHeight, float maxHeight)
{
    isAllRangeMode_ = true;
    flightAreaRadius_ = areaRadius;
    flightMinHeight_ = minHeight;
    flightMaxHeight_ = maxHeight;
    // 切り替え前の機体姿勢を引き継ぎ、飛行方向と直交する左右・上下の軸を作る。
    flightYaw_ = -transform.rotate.y;
    flightPitch_ = -transform.rotate.x;
    railForward_ = {
        std::sin(flightYaw_) * std::cos(flightPitch_),
        std::sin(flightPitch_),
        std::cos(flightYaw_) * std::cos(flightPitch_)
    };
    railRight_ = Normalize(Cross(Vector3 { 0.0f, 1.0f, 0.0f }, railForward_));
    railUp_ = Normalize(Cross(railForward_, railRight_));
    isReturningToFlightArea_ = false;
    railOffset_ = {};
    ResetSteering();
}

void PlayerMovement::UpdateAllRangeMove(EulerTransform& transform, Vector2 steering, float deltaTimeSeconds)
{
    UpdateFlightOrientation(steering, deltaTimeSeconds);
    UpdateFlightAreaReturn(transform, steering, deltaTimeSeconds);
    UpdateFlightHeight(transform, deltaTimeSeconds);
    UpdateFlightPosition(transform, deltaTimeSeconds);
    if (!IsRolling()) {
        UpdateFlightBank(transform, steering, deltaTimeSeconds);
    }
}

void PlayerMovement::UpdateFlightOrientation(const Vector2& steering, float deltaTimeSeconds)
{
    constexpr float kYawSpeed = 1.35f;
    constexpr float kPitchSpeed = 0.85f;
    constexpr float kMaxPitch = 0.95f;
    flightYaw_ += steering.x * kYawSpeed * deltaTimeSeconds;
    flightPitch_ = std::clamp(
        flightPitch_ + steering.y * kPitchSpeed * deltaTimeSeconds, -kMaxPitch, kMaxPitch);
}

void PlayerMovement::UpdateFlightAreaReturn(const EulerTransform& transform, Vector2& steering, float deltaTimeSeconds)
{
    constexpr float kYawSpeed = 1.35f;
    // 追従カメラも範囲内に収まるよう、機体が端に達する前に内側への旋回を始める。
    const float horizontalDistance = std::sqrt(
        transform.translate.x * transform.translate.x +
        transform.translate.z * transform.translate.z);
    // 復帰開始と終了の距離に差を設け、境界付近で状態が頻繁に切り替わるのを防ぐ。
    if (horizontalDistance > flightAreaRadius_ - 90.0f) {
        isReturningToFlightArea_ = true;
    } else if (horizontalDistance < flightAreaRadius_ - 150.0f) {
        isReturningToFlightArea_ = false;
    }
    if (isReturningToFlightArea_) {
        // 先に加えた手動の左右旋回を取り消し、原点方向へ自動旋回させる。
        flightYaw_ -= steering.x * kYawSpeed * deltaTimeSeconds;
        const float inwardYaw = std::atan2(-transform.translate.x, -transform.translate.z);
        const float yawDifference = std::remainder(
            inwardYaw - flightYaw_, 2.0f * std::numbers::pi_v<float>);
        flightYaw_ += std::clamp(yawDifference, -2.0f * deltaTimeSeconds, 2.0f * deltaTimeSeconds);
        steering.x = std::clamp(yawDifference, -1.0f, 1.0f);
    }
    flightYaw_ = std::remainder(flightYaw_, 2.0f * std::numbers::pi_v<float>);
}

void PlayerMovement::UpdateFlightHeight(const EulerTransform& transform, float deltaTimeSeconds)
{
    // 高度の端へ向かう場合だけ、反対向きの緩やかなピッチへ近づける。
    if (transform.translate.y < flightMinHeight_ + 12.0f && flightPitch_ < 0.0f) {
        flightPitch_ += (0.25f - flightPitch_) * (1.0f - std::exp(-4.0f * deltaTimeSeconds));
    }
    if (transform.translate.y > flightMaxHeight_ - 12.0f && flightPitch_ > 0.0f) {
        flightPitch_ += (-0.25f - flightPitch_) * (1.0f - std::exp(-4.0f * deltaTimeSeconds));
    }
}

void PlayerMovement::UpdateFlightPosition(EulerTransform& transform, float deltaTimeSeconds)
{
    railForward_ = {
        std::sin(flightYaw_) * std::cos(flightPitch_),
        std::sin(flightPitch_),
        std::cos(flightYaw_) * std::cos(flightPitch_)
    };
    railRight_ = Normalize(Cross(Vector3 { 0.0f, 1.0f, 0.0f }, railForward_));
    railUp_ = Normalize(Cross(railForward_, railRight_));
    // 60FPS基準の前進量を経過秒数に換算し、フレームレートに応じて移動させる。
    transform.translate += railForward_ * (velocity_.z * 60.0f * deltaTimeSeconds);
    const float distanceAfterMove = std::sqrt(
        transform.translate.x * transform.translate.x +
        transform.translate.z * transform.translate.z);
    if (distanceAfterMove > flightAreaRadius_) {
        // 自動復帰でも範囲外へ出た場合は、水平位置を円の境界へ戻す。
        const float correction = flightAreaRadius_ / distanceAfterMove;
        transform.translate.x *= correction;
        transform.translate.z *= correction;
    }
    transform.translate.y = std::clamp(
        transform.translate.y, flightMinHeight_, flightMaxHeight_);
    railBasePosition_ = transform.translate;
    transform.rotate.x = -flightPitch_;
    transform.rotate.y = -flightYaw_;
}

void PlayerMovement::UpdateFlightBank(EulerTransform& transform, const Vector2& steering, float deltaTimeSeconds)
{
    const float targetBank = -steering.x * 0.65f + manualBank_;
    transform.rotate.z += (targetBank - transform.rotate.z) *
        (1.0f - std::exp(-7.0f * deltaTimeSeconds));
}

void PlayerMovement::SetMouseSensitivity(float sensitivity)
{
    // 極端に小さい・大きい感度にならないよう、設定値を制限する。
    mouseSensitivity_ = std::clamp(sensitivity, 0.5f, 2.0f);
}

void PlayerMovement::UpdateSteering(const Vector2& aimPosition, const Vector2& screenSizePixels,
    bool isAllRangeMode, float deltaTimeSeconds, bool isGamepad)
{
    const float screenWidth = screenSizePixels.x;
    const float screenHeight = screenSizePixels.y;
    if (screenWidth <= 0.0f || screenHeight <= 0.0f) {
        // 画面サイズが無効なら、ゼロ除算を避けて現在の操舵入力を維持する。
        return;
    }

    // 照準をアナログスティックとして扱い、画面中央からのずれを入力へ変換する。
    // 中央は0、右端・上端は+1、左端・下端は-1。Yは画面座標と向きが逆になる。
    float inputX = (aimPosition.x - screenWidth * 0.5f) /(screenWidth * 0.5f);
    float inputY = (screenHeight * 0.5f - aimPosition.y) /(screenHeight * 0.5f);

    // 感度を掛けて-1～1へ制限し、中央付近の微小なずれを取り除く。
    // 照準の移動速度と機体の追従を分離する。パッドは照準のずれをそのまま操舵に使う。
    float sensitivity = mouseSensitivity_;
    if (isGamepad) { sensitivity = 1.0f; }
    inputX = std::clamp(inputX * sensitivity, -1.0f, 1.0f);
    inputY = std::clamp(inputY * sensitivity, -1.0f, 1.0f);
    // パッドのデッドゾーンはInputで適用済み。低感度でも細かな操作を残す。
    if (!isGamepad) {
        inputX = ApplySteeringDeadZone(inputX);
        inputY = ApplySteeringDeadZone(inputY);
    }

    // パッドは入力へ即時追従し、照準の移動・中央復帰にさらに遅れを重ねない。
    if (isGamepad) {
        steeringInput_ = { inputX, inputY };
        return;
    }
    // マウスのレール移動は即時追従し、全方向飛行では経過秒数から追従率を求める。
    // 全方向飛行の追従率は、フレームレートによる旋回感の差を抑えるため時間ベースにする。
    float steeringLerpRate = 1.0f;
    if (isAllRangeMode) {
        steeringLerpRate = 1.0f - std::exp(-13.4f * deltaTimeSeconds);
    }
    // 全方向飛行のマウス操舵には補間を適用する。
    steeringInput_.x += (inputX - steeringInput_.x) * steeringLerpRate;
    steeringInput_.y += (inputY - steeringInput_.y) * steeringLerpRate;
}

void PlayerMovement::UpdateRoll(EulerTransform& transform, bool isLeftTriggered, bool isRightTriggered, float deltaTimeSeconds)
{
    if (isRolling_) {
        // ロール中は新しい入力を判定せず、0.5秒かけてZ軸を1回転させる。
        rollTimerSeconds_ += deltaTimeSeconds;
        float progress = static_cast<float>(rollTimerSeconds_) / static_cast<float>(kRollDurationSeconds);
        transform.rotate.z = rollDirection_ * progress * 2.0f * std::numbers::pi_v<float>;

        if (rollTimerSeconds_ + 0.000001f >= kRollDurationSeconds) {
            // 1回転したら傾きを0に戻し、次のロールまで1秒待つ。
            isRolling_ = false;
            transform.rotate.z = 0.0f;
            rollCooldownSeconds_ = kRollCooldownDurationSeconds;
        }
        return;
    }

    // ロールしていない間に、再使用までの待ち時間を減らす。
    if (rollCooldownSeconds_ > 0) {
        rollCooldownSeconds_ = (std::max)(0.0f, rollCooldownSeconds_ - deltaTimeSeconds);
    }

    // 最初の押下後の受付時間を減らす。0になったら次の押下は1回目として扱う。
    if (leftKeyTapTimerSeconds_ > 0.000001f) {
        leftKeyTapTimerSeconds_ = (std::max)(0.0f, leftKeyTapTimerSeconds_ - deltaTimeSeconds);
    }
    if (rightKeyTapTimerSeconds_ > 0.000001f) {
        rightKeyTapTimerSeconds_ = (std::max)(0.0f, rightKeyTapTimerSeconds_ - deltaTimeSeconds);
    }

    // 再使用可能なときだけ、同じ方向の2回目の押下でロールを開始する。
    if (rollCooldownSeconds_ <= 0.000001f) {
        if (isLeftTriggered) {
            if (leftKeyTapTimerSeconds_ > 0.000001f) {
                isRolling_ = true;
                rollTimerSeconds_ = 0;
                rollDirection_ = 1.0f;
                leftKeyTapTimerSeconds_ = 0;
                return;
            } else {
                // 左の1回目の押下。2回目を受け付けるタイマーを開始する。
                leftKeyTapTimerSeconds_ = kMaxTapIntervalSeconds;
            }
        }

        if (isRightTriggered) {
            if (rightKeyTapTimerSeconds_ > 0.000001f) {
                isRolling_ = true;
                rollTimerSeconds_ = 0;
                rollDirection_ = -1.0f;
                rightKeyTapTimerSeconds_ = 0;
                return;
            } else {
                // 右の1回目の押下。左右の受付時間は別々に管理する。
                rightKeyTapTimerSeconds_ = kMaxTapIntervalSeconds;
            }
        }
    }
}

Vector2 PlayerMovement::CalculateScreenCorrection(const Vector3& railOffset) const
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
    Vector3 rightExtent = railRight_ * playerBoundsHalfWidth_;
    Vector3 upExtent = railUp_ * playerBoundsHalfHeight_;

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

Vector3 PlayerMovement::CalculateRailWorldPosition(const Vector3& railOffset) const
{
    // 基準位置に、レールの左右・上下・前後方向の移動量を足す。
    return railBasePosition_ + railRight_ * railOffset.x + railUp_ * railOffset.y + railForward_ * railOffset.z;
}

Vector3 PlayerMovement::ClampRailOffsetToScreen(const Vector3& railOffset) const
{
    Vector3 correctedRailOffset = railOffset;
    // 先にレール上の移動範囲へ制限する。前後方向のオフセットは使わない。
    correctedRailOffset.x = std::clamp(
        correctedRailOffset.x, -railMoveLimitX_, railMoveLimitX_);
    correctedRailOffset.y = std::clamp(
        correctedRailOffset.y, -railMoveLimitY_, railMoveLimitY_);
    correctedRailOffset.z = 0.0f;
    // Keep the configured ocean clearance after both input and screen-space correction.
    EnforceMinimumHeight(correctedRailOffset);


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
        EnforceMinimumHeight(correctedRailOffset);
    }

    return correctedRailOffset;
}

void PlayerMovement::UpdateScreenBounds(const Vector3& worldPosition, float& minX, float& maxX, float& minY, float& maxY) const
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

// 入力を取得した呼び出し側から、操舵とロールの要求を受け取る。
void PlayerMovement::UpdateMovement(EulerTransform& transform, const Vector2& steering, float railResponse,
    float manualBank, bool isLeftTriggered, bool isRightTriggered, float deltaTimeSeconds)
{
    manualBank_ = manualBank;
    if (isAllRangeMode_) {
        UpdateAllRangeMove(transform, steering, deltaTimeSeconds);
    } else {
        Vector3 nextRailOffset = railOffset_;
        nextRailOffset.x += steering.x * moveSpeed_ * railResponse
            * GameTiming::CalculateReferenceStep(deltaTimeSeconds);
        nextRailOffset.y += steering.y * moveSpeed_ * railResponse
            * GameTiming::CalculateReferenceStep(deltaTimeSeconds);
        railOffset_ = ClampRailOffsetToScreen(nextRailOffset);
    }
    UpdateRoll(transform, isLeftTriggered, isRightTriggered, deltaTimeSeconds);
}

// 入力後と画面内補正後に、設定された最低高度を維持する。
void PlayerMovement::EnforceMinimumHeight(Vector3& offset) const
{
    if (hasRailMinimumHeight_ && railUp_.y > 0.0001f) {
        const float minimumOffset = (railMinimumHeight_ - railBasePosition_.y - railRight_.y * offset.x) / railUp_.y;
        offset.y = (std::max)(offset.y, (std::min)(minimumOffset, railMoveLimitY_));
    }
}

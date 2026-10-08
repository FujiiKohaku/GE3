#include "PlayerRollController.h"
#include <numbers>
#include <algorithm>

// 入力の取得は呼び出し元に任せ、渡された左右の押下でロールを制御する。
void PlayerRollController::Update(EulerTransform& transform, bool isLeftTriggered, bool isRightTriggered, float deltaTimeSeconds)
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

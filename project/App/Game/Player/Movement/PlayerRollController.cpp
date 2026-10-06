#include "PlayerRollController.h"
#include <numbers>

void PlayerRollController::Update(EulerTransform& transform, bool isLeftTriggered, bool isRightTriggered)
{
    if (isRolling_) {
        rollTimerFrames_++;
        float progress = static_cast<float>(rollTimerFrames_) / static_cast<float>(kRollDurationFrames);
        transform.rotate.z = rollDirection_ * progress * 2.0f * std::numbers::pi_v<float>;

        if (rollTimerFrames_ >= kRollDurationFrames) {
            isRolling_ = false;
            transform.rotate.z = 0.0f;
            rollCooldownFrames_ = kRollCooldownDurationFrames;
        }
        return;
    }

    if (rollCooldownFrames_ > 0) {
        rollCooldownFrames_--;
    }

    if (leftKeyTapTimerFrames_ > 0) {
        leftKeyTapTimerFrames_--;
    }
    if (rightKeyTapTimerFrames_ > 0) {
        rightKeyTapTimerFrames_--;
    }

    if (rollCooldownFrames_ <= 0) {
        if (isLeftTriggered) {
            if (leftKeyTapTimerFrames_ > 0) {
                isRolling_ = true;
                rollTimerFrames_ = 0;
                rollDirection_ = 1.0f;
                leftKeyTapTimerFrames_ = 0;
                return;
            } else {
                leftKeyTapTimerFrames_ = kMaxTapIntervalFrames;
            }
        }

        if (isRightTriggered) {
            if (rightKeyTapTimerFrames_ > 0) {
                isRolling_ = true;
                rollTimerFrames_ = 0;
                rollDirection_ = -1.0f;
                rightKeyTapTimerFrames_ = 0;
                return;
            } else {
                rightKeyTapTimerFrames_ = kMaxTapIntervalFrames;
            }
        }
    }
}

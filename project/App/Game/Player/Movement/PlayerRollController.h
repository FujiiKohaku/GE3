#pragma once
#include "Engine/math/MathStruct.h"

class PlayerRollController {
public:
    void Update(EulerTransform& transform, bool isLeftTriggered, bool isRightTriggered);
    void Stop() { isRolling_ = false; }
    bool IsRolling() const { return isRolling_; }

private:
    bool isRolling_ = false;
    int rollTimerFrames_ = 0;
    int rollCooldownFrames_ = 0;
    float rollDirection_ = 0.0f;
    int leftKeyTapTimerFrames_ = 0;
    int rightKeyTapTimerFrames_ = 0;
    static constexpr int kMaxTapIntervalFrames = 15;
    static constexpr int kRollDurationFrames = 30;
    static constexpr int kRollCooldownDurationFrames = 60;
};

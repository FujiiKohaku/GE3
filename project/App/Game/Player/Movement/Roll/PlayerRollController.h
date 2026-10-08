#pragma once
#include "Engine/math/MathStruct.h"

class PlayerRollController {
public:
    // 使用クラス：PlayerMovementController（UpdateMovement）。ダブルタップ判定とロール回転を更新する。
    void Update(EulerTransform& transform, bool isLeftTriggered, bool isRightTriggered, float deltaTimeSeconds = 1.0f / 60.0f);
    // 使用クラス：PlayerMovementController（Stop）。ロール状態を解除する。角度と各タイマーは変更しない。
    void Stop() { isRolling_ = false; }
    // 使用クラス：PlayerMovementController（旋回時の傾き制御・外部への公開）。ロール中かを返す。
    bool IsRolling() const { return isRolling_; }

private:
    bool isRolling_ = false;
    float rollTimerSeconds_ = 0; // ロール開始からの経過秒数。
    float rollCooldownSeconds_ = 0;
    float rollDirection_ = 0.0f; // 左入力は+1、右入力は-1で、回転方向を切り替える。
    float leftKeyTapTimerSeconds_ = 0;
    float rightKeyTapTimerSeconds_ = 0;
    // 60FPSでの従来の長さを、経過秒数で管理する。
    static constexpr float kMaxTapIntervalSeconds = 0.25f;
    static constexpr float kRollDurationSeconds = 0.5f;
    static constexpr float kRollCooldownDurationSeconds = 1.0f;
};

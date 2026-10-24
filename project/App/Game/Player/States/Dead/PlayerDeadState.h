#pragma once
#include "Engine/math/MathStruct.h"

class Player;

// 死亡開始時の操作解除と、機体の落下・回転を管理する。
class PlayerDeadState {
public:
    void Enter(Player& player);
    void Update(Player& player);
    void UpdateAnimation(EulerTransform& transform, float deltaTimeSeconds);
    bool IsExplosionReady() const { return isExplosionReady_; }

private:
    float deathTimerSeconds_ = 0.0f;
    float deathFallVelocity_ = 0.0f;
    bool isExplosionReady_ = false;
    static constexpr float kDeathFallDurationSeconds = 1.2f;
};

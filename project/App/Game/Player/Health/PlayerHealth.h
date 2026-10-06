#pragma once
#include "Engine/math/MathStruct.h"

class PlayerHealth {
public:
    bool ApplyDamage(int damage, bool isRolling);
    bool Heal(int amount);
    void UpdateInvincibility();
    void UpdateDeathAnimation(EulerTransform& transform, float deltaTimeSeconds);
    bool IsDead() const { return currentHp_ <= 0; }
    bool IsAlive() const { return deathState_ == DeathState::Alive; }
    bool IsFalling() const { return deathState_ == DeathState::Falling; }
    bool IsDeathExplosionReady() const { return deathState_ == DeathState::Exploded; }
    bool ShouldDraw() const;
    int GetCurrentHp() const { return currentHp_; }
    int GetMaxHp() const { return maxHp_; }
#if defined(ENABLE_DEVELOPMENT_TOOLS)
    void SetInvincibleMode(bool isEnabled) { isInvincibleMode_ = isEnabled; }
    bool IsInvincibleMode() const { return isInvincibleMode_; }
#endif

private:
    enum class DeathState { Alive, Falling, Exploded };
    int maxHp_ = 20;
    int currentHp_ = maxHp_;
    int invincibleTimerFrames_ = 0;
#if defined(ENABLE_DEVELOPMENT_TOOLS)
    bool isInvincibleMode_ = false;
#endif
    static constexpr int kInvincibleFrames = 60;
    DeathState deathState_ = DeathState::Alive;
    float deathTimerSeconds_ = 0.0f;
    float deathFallVelocity_ = 0.0f;
    static constexpr float kDeathFallDurationSeconds = 1.2f;
};

#include "App/Game/Player/Health/PlayerHealth.h"
#include "Engine/audio/SoundManager.h"

void PlayerHealth::UpdateInvincibility()
{
    if (invincibleTimerFrames_ > 0) {
        --invincibleTimerFrames_;
    }
}

bool PlayerHealth::ShouldDraw() const
{
    const bool isVisible = invincibleTimerFrames_ <= 0 || (invincibleTimerFrames_ / 4) % 2 == 0;
    return IsFalling() || (IsAlive() && isVisible);
}

bool PlayerHealth::ApplyDamage(int damage, bool isRolling)
{
#if defined(ENABLE_DEVELOPMENT_TOOLS)
    if (isInvincibleMode_) {
        return false;
    }
#endif
    if (damage <= 0 || invincibleTimerFrames_ > 0 || isRolling || currentHp_ <= 0) {
        return false;
    }

    currentHp_ -= damage;
    SoundManager::GetInstance()->Play("PlayerDamage");
    if (currentHp_ < 0) {
        currentHp_ = 0;
    }
    if (currentHp_ == 0) {
        deathState_ = DeathState::Falling;
        deathTimerSeconds_ = 0.0f;
        deathFallVelocity_ = 0.0f;
    }
    invincibleTimerFrames_ = kInvincibleFrames;
    return true;
}

bool PlayerHealth::Heal(int amount)
{
    if (amount <= 0 || currentHp_ <= 0 || currentHp_ >= maxHp_) {
        return false;
    }

    currentHp_ += amount;
    SoundManager::GetInstance()->Play("HealPickup");
    if (currentHp_ > maxHp_) {
        currentHp_ = maxHp_;
    }

    return true;
}

void PlayerHealth::UpdateDeathAnimation(EulerTransform& transform, float deltaTimeSeconds)
{
    if (deathState_ != DeathState::Falling) {
        return;
    }

    deathTimerSeconds_ += deltaTimeSeconds;
    deathFallVelocity_ += 18.0f * deltaTimeSeconds;
    transform.translate.y -= deathFallVelocity_ * deltaTimeSeconds;
    transform.rotate.x += 1.4f * deltaTimeSeconds;
    transform.rotate.z += 3.2f * deltaTimeSeconds;

    if (deathTimerSeconds_ >= kDeathFallDurationSeconds) {
        deathState_ = DeathState::Exploded;
    }
}

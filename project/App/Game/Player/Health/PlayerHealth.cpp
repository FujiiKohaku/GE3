#include "App/Game/Player/Health/PlayerHealth.h"
#include "Engine/audio/SoundManager.h"
#include <algorithm>

// 被弾後の無敵時間を経過秒数で進める。残り時間が0になると通常状態に戻る。
void PlayerHealth::UpdateInvincibility(float deltaTimeSeconds)
{
    if (invincibleRemainingSeconds_ > 0.000001f) {
        invincibleRemainingSeconds_ = (std::max)(0.0f, invincibleRemainingSeconds_ - deltaTimeSeconds);
    }
}

// 無敵中の点滅と死亡状態に応じて描画を判定する。
bool PlayerHealth::ShouldDraw() const
{

    const bool isVisible = invincibleRemainingSeconds_ <= 0.000001f || static_cast<int>(invincibleRemainingSeconds_ * 15.0f + 0.00001f) % 2 == 0;
    return IsFalling() || (IsAlive() && isVisible);
}

// ダメージを受ける。無敵時間・ロール状態を確認し、HPを減らす。死亡時は落下演出へ進める。
bool PlayerHealth::ApplyDamage(int damage, bool isRolling)
{
#if defined(ENABLE_DEVELOPMENT_TOOLS)
    if (isInvincibleMode_) {
        return false;
    }
#endif
    if (damage <= 0 || invincibleRemainingSeconds_ > 0.000001f || isRolling || currentHp_ <= 0) {
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
    invincibleRemainingSeconds_ = kInvincibleDurationSeconds;
    return true;
}
// 生存中のHPを上限まで回復する。回復量が0以下、死亡中、またはHPが上限に達している場合は回復しない。
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
// 死亡時の落下・回転を更新する。落下演出が終わると死亡爆発へ進める。
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

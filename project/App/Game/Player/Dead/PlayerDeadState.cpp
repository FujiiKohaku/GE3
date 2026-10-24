#include "PlayerDeadState.h"
#include "App/Game/Player/Player.h"
#include "Engine/Time/TimeManager.h"

void PlayerDeadState::Enter(Player& player)
{
    deathTimerSeconds_ = 0.0f;
    deathFallVelocity_ = 0.0f;
    isExplosionReady_ = false;
    player.StopMovement();
    player.weaponController_.ResetHomingLock();
}

void PlayerDeadState::Update(Player& player)
{
    if (!isExplosionReady_) {
        UpdateAnimation(player.transform_, TimeManager::GetInstance()->GetDeltaTime());
        player.ApplyTransform();
    }
    // 発射済みの弾は死亡後も更新する。
    player.weaponController_.UpdateBullets();
    player.weaponController_.RemoveDeadBullets();
    player.object_->Update();
}

// 死亡時の落下・回転を更新する。落下演出が終わると死亡爆発へ進める。
void PlayerDeadState::UpdateAnimation(EulerTransform& transform, float deltaTimeSeconds)
{
    if (isExplosionReady_) {
        return;
    }

    deathTimerSeconds_ += deltaTimeSeconds;
    deathFallVelocity_ += 18.0f * deltaTimeSeconds;
    transform.translate.y -= deathFallVelocity_ * deltaTimeSeconds;
    transform.rotate.x += 1.4f * deltaTimeSeconds;
    transform.rotate.z += 3.2f * deltaTimeSeconds;

    if (deathTimerSeconds_ >= kDeathFallDurationSeconds) {
        isExplosionReady_ = true;
    }
}


#include "FearWormHeadChargeState.h"
#include "../FearWormBattleState.h"
#include "../../FearWormEnemy.h"
#include "App/Game/Player/Player.h"
#include "Engine/Time/TimeManager.h"
#include "Engine/Effect/EffectManager.h"
#include "Engine/3D/Object3d.h"
#include "App/Game/Enemy/Bullet/Types/NormalEnemyBullet.h"
#include <memory>
#include <utility>
#include <cmath>
#include "../State/FearWormTiming.h"
#include <algorithm>

void FearWormHeadChargeState::Reset()
{
    isFinished_ = false;
    isActive_ = false;
    isFiring_ = false;
    elapsedSeconds_ = 0.0f;
    shotTimerSeconds_ = 0.0f;
    effectTimerSeconds_ = 0.0f;
    shotCount_ = 0;
}

void FearWormHeadChargeState::Enter(FearWormBattleState& battle, FearWormEnemy& boss)
{
    Reset();
    isActive_ = true;
    battle.movementPattern_ = FearWormBattleState::MovementPattern::Drift;
    UpdateHeadAimDirection(battle, boss);
    if (battle.CanFire(boss)) {
        EffectManager::GetInstance()->PlayEffect("WormShotFlash", battle.HeadMuzzlePosition(boss));
    }
}

void FearWormHeadChargeState::Update(FearWormBattleState& battle, FearWormEnemy& boss)
{
    const float deltaTimeSeconds = TimeManager::GetInstance()->GetDeltaTime() * boss.MovementSpeedRate();
    const FearWormMovementContext context = battle.CreateMovementContext(boss, boss.player_->GetTranslate());
    battle.UpdateBattleMovement(boss, FearWormMovement::CalculateDriftTarget(context), boss.MovementSpeedRate());
    if (!battle.CanFire(boss)) {
        isFinished_ = true;
        return;
    }
    UpdateHeadAimDirection(battle, boss);
    elapsedSeconds_ += deltaTimeSeconds;
    if (!isFiring_) {
        effectTimerSeconds_ += deltaTimeSeconds;
        if (effectTimerSeconds_ >= kHeadChargeEffectIntervalSeconds) {
            effectTimerSeconds_ = std::fmod(effectTimerSeconds_, kHeadChargeEffectIntervalSeconds);
            EffectManager::GetInstance()->PlayEffect("WormShotFlash", battle.HeadMuzzlePosition(boss));
        }
        if (elapsedSeconds_ >= kHeadChargeDurationSeconds) {
            isFiring_ = true;
            shotTimerSeconds_ = kHeadChargeShotIntervalSeconds;
        }
        return;
    }
    shotTimerSeconds_ += deltaTimeSeconds;
    if (deltaTimeSeconds > 0.0f && shotTimerSeconds_ >= kHeadChargeShotIntervalSeconds) {
        shotTimerSeconds_ = std::fmod(shotTimerSeconds_, kHeadChargeShotIntervalSeconds);
        FireChargedBullet(battle, boss);
        shotCount_++;
    }
    isFinished_ = shotCount_ >= kHeadChargeShotMax;
}

void FearWormHeadChargeState::Exit(FearWormBattleState&, FearWormEnemy&)
{
    isActive_ = false;
    isFiring_ = false;
    elapsedSeconds_ = 0.0f;
    shotTimerSeconds_ = 0.0f;
    effectTimerSeconds_ = 0.0f;
    shotCount_ = 0;
}

void FearWormHeadChargeState::UpdateHeadAimDirection(FearWormBattleState& battle, FearWormEnemy& boss)
{
    if (boss.player_ == nullptr) {
        return;
    }

    if (boss.segments_.empty()) {
        return;
    }

    Vector3 targetDirection = NormalizeSafe(boss.player_->GetTranslate() - boss.segments_[0].position);

    battle.headAimDirection_ = NormalizeSafe(
        Lerp(
            battle.headAimDirection_,
            targetDirection,
            FearWormTiming::CalculateFollowBlend(kHeadAimFollowRate, TimeManager::GetInstance()->GetDeltaTime())));
}

void FearWormHeadChargeState::FireChargedBullet(FearWormBattleState& battle, FearWormEnemy& boss)
{
    if (boss.player_ == nullptr) {
        return;
    }

    if (battle.GetBulletModel() == nullptr) {
        return;
    }

    // 頭部の発射位置からプレイヤーへ向かう弾を生成する。
    std::unique_ptr<EnemyBullet> bullet = std::make_unique<NormalEnemyBullet>();
    bullet->SetFogEnabled(false);
    bullet->Initialize(battle.GetBulletModel());

    Vector3 position = battle.HeadMuzzlePosition(boss);

    Vector3 direction = NormalizeSafe(boss.player_->GetTranslate() - position);

    Vector3 velocity { };
    velocity.x = direction.x * kChargedBulletSpeed;
    velocity.y = direction.y * kChargedBulletSpeed;
    velocity.z = direction.z * kChargedBulletSpeed;

    // 通常弾より強い見た目、威力、当たり判定、寿命を設定する。
    bullet->SetScale({ kChargedBulletScale,
        kChargedBulletScale,
        kChargedBulletScale });
    bullet->SetColor(kChargedBulletColor);
    bullet->SetDamage(kChargedBulletDamage);
    bullet->SetCollisionRadius(kChargedBulletCollisionRadius);
    bullet->SetMaxLifeTime(kChargedBulletLifeTimeSeconds);
    bullet->SetTranslate(position);
    bullet->SetVelocity(velocity);

    // 管理リストへ追加して発射エフェクトを再生する。
    boss.AddEnemyBullet(std::move(bullet));

    EffectManager::GetInstance()->PlayEffect(
        "WormShotFlash",
        position);
}

Vector4 FearWormHeadChargeState::HeadAttackColor(const Vector4& baseColor) const
{
    Vector4 color = baseColor;
    if (isActive_) {
        color = kChargingHeadColor;
    }

    if (isFiring_) {
        color = kFiringHeadColor;
    }
    return color;
}

float FearWormHeadChargeState::ApplyHeadChargePulse(float basePulse) const
{
    float pulse = basePulse;
    if (isActive_) {
        pulse += kChargePulseAdd;
        pulse += std::sin(elapsedSeconds_ * kChargePulseFrequency) * kChargePulseAmplitude;
    }
    return pulse;
}

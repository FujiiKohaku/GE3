#include "FearWormLineMissileState.h"
#include "../FearWormBattleState.h"
#include "../../FearWormEnemy.h"
#include "App/Game/Player/Player.h"
#include "Engine/Time/TimeManager.h"
#include "Engine/Effect/EffectManager.h"
#include "Engine/3D/Object3d.h"
#include "App/Game/Enemy/Bullet/Types/TrackingEnemyBullet.h"
#include <memory>
#include <utility>
#include <cmath>
#include <algorithm>

void FearWormLineMissileState::Reset(size_t segmentCount)
{
    isFinished_ = false;
    formationElapsedSeconds_ = 0.0f;
    phase_ = Phase::Formation;
    phaseTimerSeconds_ = 0.0f;
    shotCount_ = 0;
    targetIndices_.clear();
    targetIndices_.reserve(segmentCount);
}

void FearWormLineMissileState::Enter(FearWormBattleState& battle, FearWormEnemy& boss)
{
    Reset(boss.GetAttackSegmentCount());
    battle.movementPattern_ = FearWormBattleState::MovementPattern::Line;
}

void FearWormLineMissileState::Update(FearWormBattleState& battle, FearWormEnemy& boss)
{
    const float deltaTimeSeconds = TimeManager::GetInstance()->GetDeltaTime();
    const FearWormMovementContext context = battle.CreateMovementContext(boss, boss.GetAttackTargetPosition());
    battle.UpdateBattleMovement(boss, CalculateTarget(context), boss.GetAttackMovementSpeedRate());
    phaseTimerSeconds_ += deltaTimeSeconds;
    switch (phase_) {
    case Phase::Formation:
        if (phaseTimerSeconds_ >= kMissileAimWaitDurationSeconds) {
            for (size_t index = 0; index < boss.GetAttackSegmentCount(); index++) {
                if (boss.IsAttackSegmentAlive(index)) {
                    targetIndices_.push_back(index);
                }
            }
            phase_ = Phase::Fire;
            phaseTimerSeconds_ = kMissileFireIntervalSeconds;
        }
        break;
    case Phase::Fire:
        if (shotCount_ >= static_cast<int32_t>(targetIndices_.size())) {
            phase_ = Phase::Recovery;
            phaseTimerSeconds_ = 0.0f;
            break;
        }
        if (deltaTimeSeconds > 0.0f && phaseTimerSeconds_ >= kMissileFireIntervalSeconds) {
            phaseTimerSeconds_ = std::fmod(phaseTimerSeconds_, kMissileFireIntervalSeconds);
            const size_t segmentIndex = targetIndices_[shotCount_];
            if (battle.CanFire(boss) && segmentIndex < boss.GetAttackSegmentCount()
                && boss.IsAttackSegmentAlive(segmentIndex)) {
                FireSingleMissile(battle, boss, segmentIndex);
            }
            shotCount_++;
        }
        break;
    case Phase::Recovery:
        isFinished_ = phaseTimerSeconds_ >= kMissilePostFireCooldownSeconds;
        break;
    }
}

void FearWormLineMissileState::Exit(FearWormBattleState&, FearWormEnemy&)
{
    targetIndices_.clear();
    shotCount_ = 0;
    phaseTimerSeconds_ = 0.0f;
    formationElapsedSeconds_ = 0.0f;
}

void FearWormLineMissileState::FireSingleMissile(FearWormBattleState& battle, FearWormEnemy& boss, size_t segmentIndex)
{
    if (!boss.HasAttackTarget() || battle.GetBulletModel() == nullptr) {
        return;
    }

    std::unique_ptr<TrackingEnemyBullet> bullet = std::make_unique<TrackingEnemyBullet>();
    bullet->Initialize(battle.GetBulletModel());
    boss.SetMissileTarget(*bullet);

    // 発射座標を設定
    bullet->SetTranslate(boss.GetAttackSegmentPosition(segmentIndex));

    // 発射エフェクトの再生
    EffectManager::GetInstance()->PlayEffect("WormShotFlash", boss.GetAttackSegmentPosition(segmentIndex));

    // ボスの弾リストに追加
    boss.SpawnAttackBullet(std::move(bullet));
}

Vector3 FearWormLineMissileState::CalculateTarget(const FearWormMovementContext& context)
{
    Vector3 result = context.playerPosition;
    result.x += kTargetOffsetX;
    result.y = kMissileAttackHeight;
    result.z += context.forwardOffset + kTargetForwardOffset;
    return result;
}

float FearWormLineMissileState::CalculateFormationBlend(float transitionSeconds)
{
    return (std::min)(transitionSeconds / kFormationDurationSeconds, 1.0f);
}

Vector3 FearWormLineMissileState::CalculateFormationPosition(const Vector3& headPosition,
    int32_t segmentOrder, float segmentSpacing, float movementElapsedSeconds,
    const Vector3& followPosition, float formationBlend)
{
    // ミサイル攻撃時の横並びと揺れを、共通の追従位置へ合成する。
    Vector3 posLine = {
        headPosition.x - (segmentSpacing * static_cast<float>(segmentOrder)),
        headPosition.y + std::sin(movementElapsedSeconds * kFormationWaveFrequency + static_cast<float>(segmentOrder) * kFormationVerticalPhaseStepRadians) * kFormationWaveScaleY,
        headPosition.z + std::cos(movementElapsedSeconds * (kFormationWaveFrequency * kFormationDepthFrequencyRate) + static_cast<float>(segmentOrder) * kFormationDepthPhaseStepRadians) * kFormationWaveScaleZ
    };

    // 補間率0では、通常の追従位置を維持する。
    return Lerp(followPosition, posLine, formationBlend);
}

void FearWormLineMissileState::UpdateFormation(float deltaTimeSeconds)
{
    formationElapsedSeconds_ += deltaTimeSeconds;
}

float FearWormLineMissileState::GetFormationBlend() const
{
    return CalculateFormationBlend(formationElapsedSeconds_);
}

Vector3 FearWormLineMissileState::CalculateFormationPosition(const FearWormFormationContext& context,
    const Vector3& followPosition, float formationBlend) const
{
    return CalculateFormationPosition(context.headPosition, context.segmentOrder,
        context.segmentSpacing, context.movementElapsedSeconds, followPosition, formationBlend);
}

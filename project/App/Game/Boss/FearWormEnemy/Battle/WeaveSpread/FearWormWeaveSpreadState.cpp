#include "FearWormWeaveSpreadState.h"
#include "../FearWormBattleState.h"
#include "../../FearWormEnemy.h"
#include "App/Game/Player/Player.h"
#include "Engine/Time/TimeManager.h"
#include "Engine/Effect/EffectManager.h"
#include "Engine/3D/Object3d.h"
#include <cmath>
#include "../State/FearWormTiming.h"
#include <algorithm>

void FearWormWeaveSpreadState::Reset()
{
    isFinished_ = false;
    elapsedSeconds_ = 0.0f;
    fireElapsedSeconds_ = 0.0f;
}

void FearWormWeaveSpreadState::Enter(FearWormBattleState& battle, FearWormEnemy&)
{
    Reset();
    battle.movementPattern_ = FearWormBattleState::MovementPattern::Weave;
}

void FearWormWeaveSpreadState::Update(FearWormBattleState& battle, FearWormEnemy& boss)
{
    const float actionDeltaSeconds = TimeManager::GetInstance()->GetDeltaTime() * boss.GetAttackMovementSpeedRate();
    elapsedSeconds_ += actionDeltaSeconds;
    const FearWormMovementContext context = battle.CreateMovementContext(boss, boss.GetAttackTargetPosition());
    battle.UpdateBattleMovement(boss, CalculateTarget(context), boss.GetAttackMovementSpeedRate());
    if (battle.CanFire(boss)) {
        const int32_t shotCount = FearWormTiming::AdvanceInterval(fireElapsedSeconds_,
            battle.kFireIntervalSeconds, TimeManager::GetInstance()->GetDeltaTime(), 3);
        for (int32_t shotIndex = 0; shotIndex < shotCount; shotIndex++) {
            const int32_t segmentIndex = battle.SelectFiringSegment(boss);
            if (segmentIndex >= 0) {
                const Vector3 position = boss.GetAttackSegmentPosition(segmentIndex);
                FireSpreadBullets(battle, boss, position, NormalizeSafe(boss.GetAttackTargetPosition() - position));
            }
        }
    }
    isFinished_ = elapsedSeconds_ >= battle.movementPatternDuration_;
}

void FearWormWeaveSpreadState::Exit(FearWormBattleState&, FearWormEnemy&)
{
    elapsedSeconds_ = 0.0f;
    fireElapsedSeconds_ = 0.0f;
}

void FearWormWeaveSpreadState::FireSpreadBullets(FearWormBattleState& battle, FearWormEnemy& boss, const Vector3& position,
    const Vector3& direction)
{
    Vector3 referenceUp = { 0.0f, 1.0f, 0.0f };
    if (std::abs(direction.y) > 0.99f) {
        referenceUp = { 0.0f, 0.0f, 1.0f };
    }
    const Vector3 right = NormalizeSafe(Cross(referenceUp, direction));
    for (int32_t shotIndex = -1; shotIndex <= 1; shotIndex++) {
        const float angle = static_cast<float>(shotIndex) * kNormalSpreadAngleRadians;
        const Vector3 shotDirection = NormalizeSafe(direction * std::cos(angle) + right * std::sin(angle));
        battle.FireDirectionalBullet(boss, position, shotDirection, false);
    }
    EffectManager::GetInstance()->PlayEffect("WormShotFlash", position);
}

Vector3 FearWormWeaveSpreadState::CalculateTarget(const FearWormMovementContext& context)
{
    Vector3 result = context.playerPosition;

    // 横方向と縦方向で異なる周期を使い、波打つ軌道を作る。
    float waveAngle = context.orbitAngle * kWeaveAngleRate;

    result.x += std::sin(waveAngle) * kWeaveWidth;
    result.y += kWeaveHeight + std::sin(waveAngle * kWeaveVerticalFrequency) * kWeaveVerticalAmplitude;
    result.z += context.forwardOffset + std::cos(waveAngle) * kWeaveDepthAmplitude;

    return result;
}

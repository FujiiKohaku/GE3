#include "FearWormDriftBurstState.h"
#include "../FearWormBattleState.h"
#include "../../FearWormEnemy.h"
#include "App/Game/Player/Player.h"
#include "Engine/Time/TimeManager.h"
#include "Engine/Effect/EffectManager.h"
#include "Engine/3D/Object3d.h"
#include <cmath>
#include "../State/FearWormTiming.h"
#include <algorithm>

void FearWormDriftBurstState::Reset()
{
    isFinished_ = false;
    elapsedSeconds_ = 0.0f;
    fireElapsedSeconds_ = 0.0f;
    segmentIndex_ = 0;
    shotsRemaining_ = 0;
    shotTimerSeconds_ = 0.0f;
    direction_ = { 0.0f, 0.0f, -1.0f };
}

void FearWormDriftBurstState::Enter(FearWormBattleState& battle, FearWormEnemy&)
{
    Reset();
    battle.movementPattern_ = FearWormBattleState::MovementPattern::Drift;
}

void FearWormDriftBurstState::Update(FearWormBattleState& battle, FearWormEnemy& boss)
{
    const float actionDeltaSeconds = TimeManager::GetInstance()->GetDeltaTime() * boss.GetAttackMovementSpeedRate();
    elapsedSeconds_ += actionDeltaSeconds;
    const FearWormMovementContext context = battle.CreateMovementContext(boss, boss.GetAttackTargetPosition());
    battle.UpdateBattleMovement(boss, FearWormMovement::CalculateDriftTarget(context), boss.GetAttackMovementSpeedRate());
    if (!battle.CanFire(boss)) {
        shotsRemaining_ = 0;
        shotTimerSeconds_ = 0.0f;
    } else if (shotsRemaining_ > 0) {
        if (segmentIndex_ < 0 || static_cast<size_t>(segmentIndex_) >= boss.GetAttackSegmentCount()
            || !boss.IsAttackSegmentAlive(segmentIndex_)) {
            shotsRemaining_ = 0;
        } else {
            const int32_t shotCount = FearWormTiming::AdvanceInterval(shotTimerSeconds_,
                kNormalBurstShotIntervalSeconds, TimeManager::GetInstance()->GetDeltaTime(), shotsRemaining_);
            for (int32_t shotIndex = 0; shotIndex < shotCount; shotIndex++) {
                battle.FireDirectionalBullet(boss, boss.GetAttackSegmentPosition(segmentIndex_), direction_);
                shotsRemaining_--;
            }
            if (shotsRemaining_ == 0) {
                fireElapsedSeconds_ = shotTimerSeconds_;
                shotTimerSeconds_ = 0.0f;
            }
        }
    } else if (elapsedSeconds_ < battle.movementPatternDuration_) {
        const int32_t shotCount = FearWormTiming::AdvanceInterval(fireElapsedSeconds_,
            battle.kFireIntervalSeconds, TimeManager::GetInstance()->GetDeltaTime(), 1);
        if (shotCount == 0) {
            return;
        }
        const int32_t segmentIndex = battle.SelectFiringSegment(boss);
        if (segmentIndex >= 0) {
            segmentIndex_ = segmentIndex;
            const Vector3 position = boss.GetAttackSegmentPosition(segmentIndex_);
            direction_ = NormalizeSafe(boss.GetAttackTargetPosition() - position);
            shotsRemaining_ = kNormalBurstShotCount - 1;
            shotTimerSeconds_ = fireElapsedSeconds_;
            fireElapsedSeconds_ = 0.0f;
            battle.FireDirectionalBullet(boss, position, direction_);
        }
    }
    // 時間切れでも開始済みの3連射を撃ち切ってから次へ進む。
    isFinished_ = elapsedSeconds_ >= battle.movementPatternDuration_ && shotsRemaining_ == 0;
}

void FearWormDriftBurstState::Exit(FearWormBattleState&, FearWormEnemy&)
{
    elapsedSeconds_ = 0.0f;
    shotsRemaining_ = 0;
    shotTimerSeconds_ = 0.0f;
    fireElapsedSeconds_ = 0.0f;
}

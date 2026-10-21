#include "FearWormDriftBurstState.h"
#include "../FearWormBattleState.h"
#include "../../FearWormEnemy.h"
#include "App/Game/Player/Player.h"
#include "Engine/Time/TimeManager.h"
#include "Engine/Effect/EffectManager.h"
#include "Engine/3D/Object3d.h"
#include <cmath>
#include <algorithm>

void FearWormDriftBurstState::Enter(FearWormBattleState& battle, FearWormEnemy&)
{
    isFinished_ = false;
    fireTimer_ = 0;
    shotsRemaining_ = 0;
    shotTimerSeconds_ = 0.0f;
    battle.movementPattern_ = FearWormBattleState::MovementPattern::Drift;
}

void FearWormDriftBurstState::Update(FearWormBattleState& battle, FearWormEnemy& boss)
{
    const float actionDeltaSeconds = TimeManager::GetInstance()->GetDeltaTime() * boss.MovementSpeedRate();
    battle.movementPatternTimer_ += actionDeltaSeconds;
    battle.UpdateBattleMovement(boss, boss.player_->GetTranslate(), boss.MovementSpeedRate());
    if (!battle.CanFire(boss)) {
        shotsRemaining_ = 0;
        shotTimerSeconds_ = 0.0f;
    } else if (shotsRemaining_ > 0) {
        if (segmentIndex_ < 0 || static_cast<size_t>(segmentIndex_) >= boss.segments_.size()
            || !boss.segments_[segmentIndex_].isAlive) {
            shotsRemaining_ = 0;
        } else {
            shotTimerSeconds_ += TimeManager::GetInstance()->GetDeltaTime();
            if (shotTimerSeconds_ >= kNormalBurstShotIntervalSeconds) {
                shotTimerSeconds_ = 0.0f;
                battle.FireDirectionalBullet(boss, boss.segments_[segmentIndex_].position, direction_);
                shotsRemaining_--;
            }
        }
    } else if (battle.movementPatternTimer_ < battle.movementPatternDuration_) {
        const int32_t segmentIndex = battle.SelectFiringSegment(boss, fireTimer_);
        if (segmentIndex >= 0) {
            segmentIndex_ = segmentIndex;
            const Vector3 position = boss.segments_[segmentIndex_].position;
            direction_ = NormalizeSafe(boss.player_->GetTranslate() - position);
            shotsRemaining_ = kNormalBurstShotCount - 1;
            shotTimerSeconds_ = 0.0f;
            battle.FireDirectionalBullet(boss, position, direction_);
        }
    }
    // 時間切れでも開始済みの3連射を撃ち切ってから次へ進む。
    isFinished_ = battle.movementPatternTimer_ >= battle.movementPatternDuration_ && shotsRemaining_ == 0;
}

void FearWormDriftBurstState::Exit(FearWormBattleState&, FearWormEnemy&)
{
    shotsRemaining_ = 0;
    shotTimerSeconds_ = 0.0f;
    fireTimer_ = 0;
}

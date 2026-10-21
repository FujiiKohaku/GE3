#include "FearWormOrbitShotState.h"
#include "../FearWormBattleState.h"
#include "../../FearWormEnemy.h"
#include "App/Game/Player/Player.h"
#include "Engine/Time/TimeManager.h"
#include "Engine/Effect/EffectManager.h"
#include "Engine/3D/Object3d.h"
#include <cmath>
#include <algorithm>

void FearWormOrbitShotState::Enter(FearWormBattleState& battle, FearWormEnemy&)
{
    isFinished_ = false;
    fireTimer_ = 0;
    battle.movementPattern_ = FearWormBattleState::MovementPattern::Orbit;
}

void FearWormOrbitShotState::Update(FearWormBattleState& battle, FearWormEnemy& boss)
{
    const float actionDeltaSeconds = TimeManager::GetInstance()->GetDeltaTime() * boss.MovementSpeedRate();
    battle.movementPatternTimer_ += actionDeltaSeconds;
    battle.UpdateBattleMovement(boss, boss.player_->GetTranslate(), boss.MovementSpeedRate());
    if (battle.CanFire(boss)) {
        const int32_t segmentIndex = battle.SelectFiringSegment(boss, fireTimer_);
        if (segmentIndex >= 0) {
            battle.FireBullet(boss, boss.segments_[segmentIndex].position);
        }
    }
    isFinished_ = battle.movementPatternTimer_ >= battle.movementPatternDuration_;
}

void FearWormOrbitShotState::Exit(FearWormBattleState&, FearWormEnemy&)
{
    fireTimer_ = 0;
}

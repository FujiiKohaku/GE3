#include "FearWormOrbitShotState.h"
#include "../FearWormBattleState.h"
#include "../../FearWormEnemy.h"
#include "App/Game/Player/Player.h"
#include "Engine/Time/TimeManager.h"
#include "Engine/Effect/EffectManager.h"
#include "Engine/3D/Object3d.h"
#include <cmath>
#include "../State/FearWormTiming.h"
#include <algorithm>

void FearWormOrbitShotState::Reset()
{
    isFinished_ = false;
    elapsedSeconds_ = 0.0f;
    fireElapsedSeconds_ = 0.0f;
}

void FearWormOrbitShotState::Enter(FearWormBattleState& battle, FearWormEnemy&)
{
    Reset();
    battle.movementPattern_ = FearWormBattleState::MovementPattern::Orbit;
}

void FearWormOrbitShotState::Update(FearWormBattleState& battle, FearWormEnemy& boss)
{
    const float actionDeltaSeconds = TimeManager::GetInstance()->GetDeltaTime() * boss.MovementSpeedRate();
    elapsedSeconds_ += actionDeltaSeconds;
    const FearWormMovementContext context = battle.CreateMovementContext(boss, boss.player_->GetTranslate());
    battle.UpdateBattleMovement(boss, FearWormMovement::CalculateOrbitTarget(context), boss.MovementSpeedRate());
    if (battle.CanFire(boss)) {
        const int32_t shotCount = FearWormTiming::AdvanceInterval(fireElapsedSeconds_,
            battle.kFireIntervalSeconds, TimeManager::GetInstance()->GetDeltaTime(), 3);
        for (int32_t shotIndex = 0; shotIndex < shotCount; shotIndex++) {
            const int32_t segmentIndex = battle.SelectFiringSegment(boss);
            if (segmentIndex >= 0) {
                battle.FireBullet(boss, boss.segments_[segmentIndex].position);
            }
        }
    }
    isFinished_ = elapsedSeconds_ >= battle.movementPatternDuration_;
}

void FearWormOrbitShotState::Exit(FearWormBattleState&, FearWormEnemy&)
{
    elapsedSeconds_ = 0.0f;
    fireElapsedSeconds_ = 0.0f;
}

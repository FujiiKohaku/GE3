#include "FearWormWaitForBodyState.h"
#include "../FearWormEntryState.h"
#include "../../FearWormEnemy.h"

void FearWormWaitForBodyState::Update(FearWormEntryState& entry, FearWormEnemy& boss)
{
    entry.UpdateSegmentPositions(boss);
    if (entry.elapsedSeconds_ >= entry.durationSeconds_) {
        boss.ChangeState(FearWormEnemy::BossState::Battle);
    }
}

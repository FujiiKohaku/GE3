#include "FearWormBattleState.h"
#include "../FearWormEnemy.h"
#include "App/Game/Player/Player.h"

void FearWormBattleState::Update(FearWormEnemy& boss)
{
    if (boss.player_ == nullptr) {
        return;
    }

    float movementSpeedRate = boss.MovementSpeedRate();
    Vector3 playerPosition = boss.player_->GetTranslate();

    boss.UpdateMadMode(movementSpeedRate);
    boss.UpdateMissileAttack();
    boss.UpdateBattleMovement(playerPosition, movementSpeedRate);
    boss.UpdateBattleAttack();
}

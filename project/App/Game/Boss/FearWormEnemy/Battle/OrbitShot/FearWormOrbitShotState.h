#pragma once

#include "../State/FearWormBattleAction.h"
#include "Engine/Math/MathStruct.h"
#include <cstdint>

class FearWormOrbitShotState final : public FearWormBattleAction {
public:
    void Enter(FearWormBattleState& battle, FearWormEnemy& boss) override;
    void Update(FearWormBattleState& battle, FearWormEnemy& boss) override;
    void Exit(FearWormBattleState& battle, FearWormEnemy& boss) override;

private:
    friend class FearWormBattleState;
    friend bool RunFearWormStateTests();
    friend bool RunFearWormBattleActionTests();
    int32_t fireTimer_ = 0;
};

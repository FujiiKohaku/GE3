#pragma once

#include "../State/FearWormBattleAction.h"
#include "Engine/Math/MathStruct.h"
#include <cstdint>

class FearWormSpiralBarrageState final : public FearWormBattleAction {
public:
    void Enter(FearWormBattleState& battle, FearWormEnemy& boss) override;
    void Update(FearWormBattleState& battle, FearWormEnemy& boss) override;
    void Exit(FearWormBattleState& battle, FearWormEnemy& boss) override;

private:
    friend bool RunFearWormStateTests();
    friend bool RunFearWormBattleActionTests();
    void FireBarrage(FearWormBattleState& battle, FearWormEnemy& boss);
    float elapsedSeconds_ = 0.0f;
    float spiralAngle_ = 0.0f;
    float barrageAngle_ = 0.0f;
    int32_t fireTimer_ = 0;
};

#pragma once

#include "../State/FearWormBattleAction.h"
#include "Engine/Math/MathStruct.h"
#include <cstdint>

class FearWormOrbitShotState final : public FearWormBattleAction {
public:
    void Reset();
    void Enter(FearWormBattleState& battle, FearWormEnemy& boss) override;
    void Update(FearWormBattleState& battle, FearWormEnemy& boss) override;
    void Exit(FearWormBattleState& battle, FearWormEnemy& boss) override;

private:
    float elapsedSeconds_ = 0.0f;
    friend bool RunFearWormStateTests();
    friend bool RunFearWormBattleActionTests();
    float fireElapsedSeconds_ = 0.0f;
};

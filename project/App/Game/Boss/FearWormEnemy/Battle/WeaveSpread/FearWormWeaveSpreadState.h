#pragma once

#include "../State/FearWormBattleAction.h"
#include "Engine/Math/MathStruct.h"
#include <cstdint>

class FearWormWeaveSpreadState final : public FearWormBattleAction {
public:
    void Enter(FearWormBattleState& battle, FearWormEnemy& boss) override;
    void Update(FearWormBattleState& battle, FearWormEnemy& boss) override;
    void Exit(FearWormBattleState& battle, FearWormEnemy& boss) override;

private:
    static constexpr float kNormalSpreadAngleRadians = 0.14f;

    friend class FearWormBattleState;
    friend bool RunFearWormStateTests();
    friend bool RunFearWormBattleActionTests();
    void FireSpreadBullets(FearWormBattleState& battle, FearWormEnemy& boss,
        const Vector3& position, const Vector3& direction);
    int32_t fireTimer_ = 0;
};

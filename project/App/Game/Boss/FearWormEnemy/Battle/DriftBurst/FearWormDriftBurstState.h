#pragma once

#include "../State/FearWormBattleAction.h"
#include "Engine/Math/MathStruct.h"
#include <cstdint>

class FearWormDriftBurstState final : public FearWormBattleAction {
public:
    void Enter(FearWormBattleState& battle, FearWormEnemy& boss) override;
    void Update(FearWormBattleState& battle, FearWormEnemy& boss) override;
    void Exit(FearWormBattleState& battle, FearWormEnemy& boss) override;

private:
    static constexpr int32_t kNormalBurstShotCount = 3;
    static constexpr float kNormalBurstShotIntervalSeconds = 0.12f;

    friend class FearWormBattleState;
    friend bool RunFearWormStateTests();
    friend bool RunFearWormBattleActionTests();
    int32_t fireTimer_ = 0;
    int32_t segmentIndex_ = 0;
    int32_t shotsRemaining_ = 0;
    float shotTimerSeconds_ = 0.0f;
    Vector3 direction_ = { 0.0f, 0.0f, -1.0f };
};

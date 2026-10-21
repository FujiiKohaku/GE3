#pragma once

#include "../State/FearWormBattleAction.h"
#include "Engine/Math/MathStruct.h"
#include <cstdint>

class FearWormCoilBarrageState final : public FearWormBattleAction {
public:
    void Enter(FearWormBattleState& battle, FearWormEnemy& boss) override;
    void Update(FearWormBattleState& battle, FearWormEnemy& boss) override;
    void Exit(FearWormBattleState& battle, FearWormEnemy& boss) override;

private:
    static constexpr float kCoilBarrageFireIntervalSeconds = 0.6f;
    static constexpr int32_t kCoilBarrageBulletCount = 6;
    static constexpr float kCoilBarrageSegmentAngleRadians = 0.17f;
    static constexpr float kCoilBarrageAngularSpeedRadiansPerSecond = 0.35f;
    static constexpr float kCoilBarrageSpreadRatio = 0.35f;

    friend class FearWormBattleState;
    friend bool RunFearWormStateTests();
    friend bool RunFearWormBattleActionTests();
    void FireBarrage(FearWormBattleState& battle, FearWormEnemy& boss);
    float nextFireSeconds_ = 0.0f;
};

#pragma once

#include "../State/FearWormBattleAction.h"
#include "../Movement/FearWormMovement.h"
#include "Engine/Math/MathStruct.h"
#include <cstdint>

class FearWormSpiralBarrageState final : public FearWormBattleAction {
public:
    static Vector3 CalculateTarget(const FearWormMovementContext& context,
        float elapsedSeconds, float spiralAngleRadians);
    void Reset();
    void Enter(FearWormBattleState& battle, FearWormEnemy& boss) override;
    void Update(FearWormBattleState& battle, FearWormEnemy& boss) override;
    void Exit(FearWormBattleState& battle, FearWormEnemy& boss) override;

private:
    static constexpr float kThreatDurationSeconds = 2.0f;
    static constexpr float kSpiralDurationSeconds = 5.0f;
    static constexpr float kForwardOffsetAdd = 50.0f;
    static constexpr float kHeight = 7.0f;
    static constexpr float kMaximumRadius = 25.0f;
    static constexpr float kMinimumHeight = 1.5f;
    static constexpr float kFireIntervalSeconds = 0.05f;
    static constexpr int32_t kMaxShotsPerUpdate = 3;
    friend bool RunFearWormStateTests();
    friend bool RunFearWormBattleActionTests();
    void FireBarrage(FearWormBattleState& battle, FearWormEnemy& boss);
    float elapsedSeconds_ = 0.0f;
    float spiralAngle_ = 0.0f;
    float barrageAngle_ = 0.0f;
    float fireElapsedSeconds_ = 0.0f;
};

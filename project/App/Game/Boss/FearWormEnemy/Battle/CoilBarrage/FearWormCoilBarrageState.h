#pragma once

#include "../State/FearWormBattleAction.h"
#include "../Movement/FearWormMovement.h"
#include "Engine/Math/MathStruct.h"
#include <cstdint>

class FearWormCoilBarrageState final : public FearWormBattleAction {
public:
    static Vector3 CalculateTarget(const FearWormMovementContext& context, float elapsedSeconds);
    static float CalculateFormationBlend(float elapsedSeconds);
    static bool IsFiring(float elapsedSeconds);
    static Vector3 CalculateSegmentPosition(const Vector3& headPosition, float orbitAngle,
        int32_t segmentOrder, float segmentSpacing);
    static Vector3 CalculateFormationPosition(const Vector3& headPosition, float orbitAngle,
        int32_t segmentOrder, float segmentSpacing, const Vector3& followPosition, float elapsedSeconds);
    Vector3 CalculateFormationPosition(const Vector3& headPosition, float orbitAngle,
        int32_t segmentOrder, float segmentSpacing, const Vector3& followPosition) const;
    void Reset();
    void Enter(FearWormBattleState& battle, FearWormEnemy& boss) override;
    void Update(FearWormBattleState& battle, FearWormEnemy& boss) override;
    void Exit(FearWormBattleState& battle, FearWormEnemy& boss) override;

private:
    float elapsedSeconds_ = 0.0f;
    static constexpr float kCoilAngleRate = 2.35f;
    static constexpr float kCoilBaseRadius = 7.0f;
    static constexpr float kCoilHeight = 30.0f;
    static constexpr float kCoilSegmentAngleRadians = 0.65f;
    static constexpr float kCoilSegmentHeightRate = 0.46f;
    static constexpr float kCoilFormationDurationSeconds = 1.1f;
    static constexpr float kCoilAttackDurationSeconds = 2.4f;
    static constexpr float kCoilReleaseDurationSeconds = 0.8f;
    static constexpr float kCoilBarrageFireIntervalSeconds = 0.6f;
    static constexpr int32_t kCoilBarrageBulletCount = 6;
    static constexpr float kCoilBarrageSegmentAngleRadians = 0.17f;
    static constexpr float kCoilBarrageAngularSpeedRadiansPerSecond = 0.35f;
    static constexpr float kCoilBarrageSpreadRatio = 0.35f;

    friend bool RunFearWormMovementTests();
    friend bool RunFearWormStateTests();
    friend bool RunFearWormBattleActionTests();
    void FireBarrage(FearWormBattleState& battle, FearWormEnemy& boss);
    float nextFireSeconds_ = 0.0f;
};

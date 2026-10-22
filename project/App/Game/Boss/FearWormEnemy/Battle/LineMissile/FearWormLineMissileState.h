#pragma once

#include "../State/FearWormBattleAction.h"
#include "../Movement/FearWormMovement.h"
#include "Engine/Math/MathStruct.h"
#include <cstdint>
#include <cstddef>
#include <vector>

class FearWormLineMissileState final : public FearWormBattleAction {
public:
    static Vector3 CalculateTarget(const FearWormMovementContext& context);
    static float CalculateFormationBlend(float transitionSeconds);
    static Vector3 CalculateFormationPosition(const Vector3& headPosition, int32_t segmentOrder,
        float segmentSpacing, float movementElapsedSeconds, const Vector3& followPosition, float formationBlend);
    float UpdateFormationBlend(float deltaTimeSeconds);
    void Reset(size_t segmentCount);
    void Enter(FearWormBattleState& battle, FearWormEnemy& boss) override;
    void Update(FearWormBattleState& battle, FearWormEnemy& boss) override;
    void Exit(FearWormBattleState& battle, FearWormEnemy& boss) override;

private:
    static constexpr float kTargetOffsetX = 20.0f;
    static constexpr float kTargetOffsetY = 3.0f;
    static constexpr float kTargetForwardOffset = 75.0f;
    static constexpr float kFormationDurationSeconds = 1.0f;
    static constexpr float kFormationWaveScaleY = 3.2f;
    static constexpr float kFormationWaveScaleZ = 2.0f;
    static constexpr float kFormationWaveFrequency = 4.2f;
    static constexpr float kFormationDepthFrequencyRate = 0.8f;
    static constexpr float kFormationVerticalPhaseStepRadians = 0.8f;
    static constexpr float kFormationDepthPhaseStepRadians = 0.6f;
    static constexpr float kMissileAimWaitDurationSeconds = 2.0f;   // 整列完了（前準備）を待つ時間（秒）
    static constexpr float kMissileFireIntervalSeconds = 0.45f;    // ミサイルの発射間隔（秒）
    static constexpr float kMissilePostFireCooldownSeconds = 1.8f; // 撃ち終わった後の隙（秒）

    friend bool RunFearWormMovementTests();
    friend bool RunFearWormStateTests();
    friend bool RunFearWormBattleActionTests();
    void FireSingleMissile(FearWormBattleState& battle, FearWormEnemy& boss, size_t segmentIndex);
    enum class Phase { Formation, Fire, Recovery };
    Phase phase_ = Phase::Formation;
    float phaseTimerSeconds_ = 0.0f;
    float formationElapsedSeconds_ = 0.0f;
    int32_t shotCount_ = 0;
    std::vector<size_t> targetIndices_;
};

#pragma once
#include "Engine/Math/MathStruct.h"
#include <cstdint>

class FearWormBattleState;
class FearWormEnemy;

struct FearWormFormationContext {
    Vector3 headPosition;
    float orbitAngleRadians;
    int32_t segmentOrder;
    float segmentSpacing;
    float movementElapsedSeconds;
};

// 移動と攻撃を一組として実行する、戦闘中の行動。
class FearWormBattleAction {
public:
    virtual ~FearWormBattleAction() = default;
    virtual void Enter(FearWormBattleState& battle, FearWormEnemy& boss) = 0;
    virtual void Update(FearWormBattleState& battle, FearWormEnemy& boss) = 0;
    virtual void Exit(FearWormBattleState& battle, FearWormEnemy& boss) = 0;
    // 行動の切り替え後に1回更新し、胴体側からは結果だけを参照する。
    virtual void UpdateFormation(float) {}
    virtual float GetFormationBlend() const { return 0.0f; }
    virtual Vector3 CalculateFormationPosition(const FearWormFormationContext&,
        const Vector3& followPosition, float) const { return followPosition; }
    // 行動固有の姿勢と見た目。通常行動では元の状態を維持する。
    virtual bool ShouldAimHead(const FearWormEnemy&) const { return false; }
    virtual Vector4 HeadAttackColor(const Vector4& baseColor) const { return baseColor; }
    virtual float ApplyHeadChargePulse(float basePulse) const { return basePulse; }
    bool IsFinished() const { return isFinished_; }

protected:
    bool isFinished_ = false;
};

#pragma once
#include "Engine/Math/MathStruct.h"

class FearWormBattleState;
class FearWormEnemy;

// 移動と攻撃を一組として実行する、戦闘中の行動。
class FearWormBattleAction {
public:
    virtual ~FearWormBattleAction() = default;
    virtual void Enter(FearWormBattleState& battle, FearWormEnemy& boss) = 0;
    virtual void Update(FearWormBattleState& battle, FearWormEnemy& boss) = 0;
    virtual void Exit(FearWormBattleState& battle, FearWormEnemy& boss) = 0;
    // 行動固有の姿勢と見た目。通常行動では元の状態を維持する。
    virtual bool ShouldAimHead(const FearWormEnemy&) const { return false; }
    virtual Vector4 HeadAttackColor(const Vector4& baseColor) const { return baseColor; }
    virtual float ApplyHeadChargePulse(float basePulse) const { return basePulse; }
    bool IsFinished() const { return isFinished_; }

protected:
    bool isFinished_ = false;
};

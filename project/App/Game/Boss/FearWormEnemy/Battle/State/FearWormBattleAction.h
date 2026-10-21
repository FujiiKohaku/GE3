#pragma once

class FearWormBattleState;
class FearWormEnemy;

// 移動と攻撃を一組として実行する、戦闘中の行動。
class FearWormBattleAction {
public:
    virtual ~FearWormBattleAction() = default;
    virtual void Enter(FearWormBattleState& battle, FearWormEnemy& boss) = 0;
    virtual void Update(FearWormBattleState& battle, FearWormEnemy& boss) = 0;
    virtual void Exit(FearWormBattleState& battle, FearWormEnemy& boss) = 0;
    bool IsFinished() const { return isFinished_; }

protected:
    bool isFinished_ = false;
};

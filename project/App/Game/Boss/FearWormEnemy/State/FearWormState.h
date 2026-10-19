#pragma once

class FearWormEnemy;

// ボスの状態に共通する開始・更新・終了処理。
// 各状態はボスごとに所有し、必要な状態データも保持する。
class FearWormState {
public:
    virtual ~FearWormState() = default;
    virtual void Enter(FearWormEnemy&) {}
    virtual void Update(FearWormEnemy& boss) = 0;
    virtual void Exit(FearWormEnemy&) {}
};

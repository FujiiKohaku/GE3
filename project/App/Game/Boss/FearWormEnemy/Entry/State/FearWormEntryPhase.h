#pragma once

class FearWormEnemy;
class FearWormEntryState;

// 登場内の各段階に共通する開始・更新・終了処理。
class FearWormEntryPhase {
public:
    virtual ~FearWormEntryPhase() = default;
    virtual void Enter(FearWormEntryState&, FearWormEnemy&) {}
    virtual void Update(FearWormEntryState& entry, FearWormEnemy& boss) = 0;
    virtual void Exit(FearWormEntryState&, FearWormEnemy&) {}
};

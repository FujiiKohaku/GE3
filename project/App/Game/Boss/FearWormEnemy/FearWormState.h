#pragma once

#include "FearWormEnemy.h"

// FearWorm専用の状態。振る舞いを担当し、タイマー等はボス本体が保持する。
class FearWormState {
public:
    using BossState = FearWormEnemy::BossState;

    virtual ~FearWormState() = default;
    virtual BossState GetType() const = 0;
    virtual void Enter(FearWormEnemy&) const {}
    virtual void Exit(FearWormEnemy&) const {}
    virtual void Update(FearWormEnemy& boss) const = 0;
};


#pragma once

#include "../State/IceJellyfishState.h"
#include <cstdint>

// 浮遊移動と、並行して進行できる3攻撃の開始順序を管理する。
class IceJellyfishBattleState final : public IceJellyfishState {
public:
    void Reset();
    void Update(IceJellyfish& boss) override;

private:
    friend bool RunJellyfishAttackTests();
    friend bool RunIceJellyfishStateTests();
    void UpdateAttackSequence(IceJellyfish& boss, float floatPhase, float deltaTimeSeconds);
    int32_t nextAttackIndex_ = 0;
    bool isPulseAttackArmed_ = false;
};

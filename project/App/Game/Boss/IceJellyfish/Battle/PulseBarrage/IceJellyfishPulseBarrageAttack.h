#pragma once

#include "../Common/IceJellyfishAttackMotion.h"
#include <array>
#include <cstddef>
#include <cstdint>

class IceJellyfish;
class IceJellyfishBattleState;

class IceJellyfishPulseBarrageAttack final {
public:
    void Reset();
    void Start();
    void Update(IceJellyfish& boss, float deltaTimeSeconds);

private:
    friend bool RunJellyfishAttackTests();
    friend bool RunIceJellyfishStateTests();
    void FireRing(IceJellyfish& boss,
        int count, float spread, float speed, float angleOffset, bool includeCenter);
    int32_t pulseBarrageWave_ = -1;
    float pulseBarrageTimer_ = 0.0f;
};

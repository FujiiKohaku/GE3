#pragma once

#include "../State/FearWormState.h"

class FearWormDeadState final : public FearWormState {
public:
    void Enter(FearWormEnemy& boss) override;
    void Update(FearWormEnemy& boss) override;
};

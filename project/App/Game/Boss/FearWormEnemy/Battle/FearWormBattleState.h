#pragma once

#include "../State/FearWormState.h"

class FearWormBattleState final : public FearWormState {
public:
    void Update(FearWormEnemy& boss) override;
};

#pragma once

#include "../State/FearWormState.h"

class FearWormWaitState final : public FearWormState {
public:
    void Update(FearWormEnemy& boss) override;
};

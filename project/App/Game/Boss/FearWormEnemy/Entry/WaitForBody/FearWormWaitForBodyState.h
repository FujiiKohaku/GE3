#pragma once

#include "../State/FearWormEntryPhase.h"

class FearWormWaitForBodyState final : public FearWormEntryPhase {
public:
    void Update(FearWormEntryState& entry, FearWormEnemy& boss) override;
};

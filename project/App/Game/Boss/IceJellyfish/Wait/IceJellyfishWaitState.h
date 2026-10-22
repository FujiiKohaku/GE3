#pragma once

#include "../State/IceJellyfishState.h"

// 出現条件はEncounter側で判定済み。最初の更新で登場へ進む。
class IceJellyfishWaitState final : public IceJellyfishState {
public:
    void Update(IceJellyfish& boss) override;
};

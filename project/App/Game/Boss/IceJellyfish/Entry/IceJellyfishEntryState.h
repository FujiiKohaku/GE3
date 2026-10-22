#pragma once

#include "../State/IceJellyfishState.h"

// 登場演出は未実装。時間を消費せず戦闘へ進む。
class IceJellyfishEntryState final : public IceJellyfishState {
public:
    void Update(IceJellyfish& boss) override;
};

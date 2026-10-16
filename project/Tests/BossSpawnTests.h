#pragma once
#include "App/Game/Boss/Encounter/BossEncounterController.h"

inline bool RunBossSpawnTests()
{
    BossEncounterController encounter;
    encounter.Initialize("None", 0.0f, {}, nullptr, nullptr, nullptr, nullptr, nullptr, false, 0.0f);
    encounter.Update(100.0f);
    return !encounter.IsSpawned() && encounter.GetActiveBoss() == nullptr && !encounter.DidSpawnThisFrame();
}

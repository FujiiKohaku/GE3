#pragma once
#include "App/Game/Boss/Encounter/BaseBossFactory.h"
#include "App/Game/Boss/Encounter/BossEncounterController.h"

inline bool RunBossFactoryTests()
{
    for (const auto type : { "AngerBlock", "IceJellyfish", "FearWorm" }) {
        const BaseBossFactory* factory = FindBossFactory(type);
        if (!factory || factory->GetType() != type || FindBossFactory(type) != factory) return false;
    }
    if (FindBossFactory("None") != nullptr || FindBossFactory("Unknown") != FindBossFactory("FearWorm") ||
        FindBossFactory("") != FindBossFactory("FearWorm")) return false;
    BossEncounterController encounter;
    encounter.Initialize("None", 0.0f, {}, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, false, 0.0f);
    encounter.Update(100.0f);
    return !encounter.IsSpawned() && encounter.GetActiveBoss() == nullptr && !encounter.DidSpawnThisFrame();
}

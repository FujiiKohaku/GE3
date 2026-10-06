#include "App/Game/Player/Weapon/PlayerWeaponHeat.h"
#include <cassert>
#include <cmath>

namespace {
void FillHeat(PlayerWeaponHeat& heat)
{
    for (int shotIndex = 0; shotIndex < 13; ++shotIndex) {
        heat.AddShotHeat(false);
    }
    assert(heat.GetHeatRatio() == 1.0f);
}

void TestGraceAndCompleteRecovery()
{
    PlayerWeaponHeat heat;
    FillHeat(heat);
    assert(!heat.IsOverheated());
    heat.Update(0.25f, true);
    heat.AddShotHeat(true);
    heat.Update(0.24f, true);
    assert(!heat.IsOverheated());
    assert(heat.GetHeatRatio() == 1.0f);
    heat.Update(0.02f, true);
    assert(heat.IsOverheated());
    heat.Update(3.0f, true);
    assert(heat.GetHeatRatio() == 0.25f);
    assert(heat.IsOverheated());
    heat.AddShotHeat(false);
    assert(heat.GetHeatRatio() == 0.25f);
    heat.Update(0.99f, false);
    assert(heat.IsOverheated());
    heat.Update(0.02f, false);
    assert(heat.GetHeatRatio() == 0.0f);
    assert(!heat.IsOverheated());
    heat.AddShotHeat(false);
    assert(std::abs(heat.GetHeatRatio() - 0.08f) < 0.0001f);
}

void TestReleaseCancelsGrace()
{
    PlayerWeaponHeat heat;
    FillHeat(heat);
    heat.Update(0.4f, true);
    heat.Update(0.1f, false);
    assert(!heat.IsOverheated());
    assert(heat.GetHeatRatio() < 1.0f);
    heat.AddShotHeat(false);
    heat.Update(0.4f, true);
    assert(!heat.IsOverheated());
    heat.Update(0.11f, true);
    assert(heat.IsOverheated());
}

void TestPausedTimeAndShotHeat()
{
    PlayerWeaponHeat heat;
    heat.AddShotHeat(true);
    assert(std::abs(heat.GetHeatRatio() - 0.04f) < 0.0001f);
    heat.Update(1.0f, false);
    assert(heat.GetHeatRatio() == 0.0f);
    FillHeat(heat);
    heat.Update(0.0f, true);
    heat.Update(-1.0f, false);
    assert(heat.GetHeatRatio() == 1.0f);
    heat.Update(0.49f, true);
    assert(!heat.IsOverheated());
    heat.Update(0.02f, true);
    assert(heat.IsOverheated());
}
}

int main()
{
    TestGraceAndCompleteRecovery();
    TestReleaseCancelsGrace();
    TestPausedTimeAndShotHeat();
}

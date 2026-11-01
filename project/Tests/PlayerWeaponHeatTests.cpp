#include "App/Game/Player/Combat/Weapon/PlayerWeaponController.h"
#include "Tests/PlayerWeaponResetTests.h"
#include "Tests/PlayerWeaponPolymorphismTests.h"
#include <cassert>
#include <cmath>

namespace {
void FillHeat(PlayerWeaponHeat& heat)
{
    for (int shotIndex = 0; shotIndex < 41; ++shotIndex) {
        heat.AddShotHeat();
    }
    assert(heat.GetHeatRatio() == 1.0f);
}

void TestGraceAndCompleteRecovery()
{
    PlayerWeaponHeat heat;
    FillHeat(heat);
    assert(!heat.IsOverheated());
    heat.Update(0.25f, true);
    heat.AddShotHeat();
    heat.Update(0.24f, true);
    assert(!heat.IsOverheated());
    assert(heat.GetHeatRatio() == 1.0f);
    heat.Update(0.02f, true);
    assert(heat.IsOverheated());
    heat.Update(3.0f, true);
    assert(heat.GetHeatRatio() == 0.25f);
    assert(heat.IsOverheated());
    heat.AddShotHeat();
    assert(heat.GetHeatRatio() == 0.25f);
    heat.Update(0.99f, false);
    assert(heat.IsOverheated());
    heat.Update(0.02f, false);
    assert(heat.GetHeatRatio() == 0.0f);
    assert(!heat.IsOverheated());
    heat.AddShotHeat();
    assert(std::abs(heat.GetHeatRatio() - 0.025f) < 0.0001f);
}

void TestReleaseCancelsGrace()
{
    PlayerWeaponHeat heat;
    FillHeat(heat);
    heat.Update(0.4f, true);
    heat.Update(0.1f, false);
    assert(!heat.IsOverheated());
    assert(heat.GetHeatRatio() < 1.0f);
    heat.AddShotHeat();
    heat.Update(0.4f, true);
    assert(!heat.IsOverheated());
    heat.Update(0.11f, true);
    assert(heat.IsOverheated());
}

void TestPausedTimeAndShotHeat()
{
    PlayerWeaponHeat heat;
    heat.AddShotHeat();
    assert(std::abs(heat.GetHeatRatio() - 0.025f) < 0.0001f);
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

void TestHeatDamageAndCooling()
{
    PlayerWeaponHeat heat;
    assert(heat.GetShotDamage(3) == 3);
    assert(heat.GetShotDamage(1) == 1);
    int previousDamage = 3;
    for (int shot = 0; shot < 41; ++shot) {
        heat.AddShotHeat();
        const int damage = heat.GetShotDamage(3);
        assert(damage >= previousDamage && damage <= 6);
        previousDamage = damage;
    }
    assert(heat.GetShotDamage(3) == 6);
    assert(heat.GetShotDamage(1) == 2);
    heat.Update(2.5f, false);
    assert(std::abs(heat.GetHeatRatio() - 0.5f) < 0.0001f);
    assert(heat.GetShotDamage(3) == 5);
    assert(heat.GetShotDamage(1) == 2);
    heat.Update(2.5f, false);
    assert(heat.GetShotDamage(3) == 3);
    assert(heat.GetShotDamage(1) == 1);
}

void TestAssaultSustainedFireCanOverheat()
{
    PlayerWeaponHeat heat;
    for (int shot = 0; shot < 100; ++shot) {
        heat.Update(0.05f, true);
        if (!heat.IsOverheated()) heat.AddShotHeat();
        else break;
    }
    assert(heat.IsOverheated());
}
}

int main()
{
    assert(RunPlayerWeaponResetTests());
    assert(RunPlayerWeaponPolymorphismTests());
    TestGraceAndCompleteRecovery();
    TestReleaseCancelsGrace();
    TestPausedTimeAndShotHeat();
    TestHeatDamageAndCooling();
    TestAssaultSustainedFireCanOverheat();
}

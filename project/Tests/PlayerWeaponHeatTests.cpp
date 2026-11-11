#include "App/Game/Player/Player.h"
#include "Tests/PlayerWeaponResetTests.h"
#include "Tests/PlayerWeaponPolymorphismTests.h"
#include <cassert>
#include <cmath>

struct PlayerWeaponHeatTestAccess {
    Player player;
    PlayerWeaponController controller{player};
    void Update(float deltaTimeSeconds, bool isHeld) { controller.UpdateHeat(deltaTimeSeconds, isHeld); }
    void AddShotHeat() { controller.AddShotHeat(); }
    float GetHeatRatio() const { return controller.GetHeatRatio(); }
    bool IsOverheated() const { return controller.IsOverheated(); }
    int GetShotDamage(int damage) const { return controller.GetShotDamage(damage); }
};
namespace {
void FillHeat(PlayerWeaponHeatTestAccess& heat)
{
    constexpr int kMaxShots = 1000;
    for (int shotIndex = 0; shotIndex < kMaxShots && heat.GetHeatRatio() < 1.0f; ++shotIndex) {
        heat.AddShotHeat();
    }
    assert(heat.GetHeatRatio() == 1.0f);
}

void TestGraceAndCompleteRecovery()
{
    PlayerWeaponHeatTestAccess heat;
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
    assert(heat.GetHeatRatio() > 0.0f && heat.GetHeatRatio() < 1.0f);
}

void TestReleaseCancelsGrace()
{
    PlayerWeaponHeatTestAccess heat;
    FillHeat(heat);
    heat.Update(0.4f, true);
    heat.Update(0.1f, false);
    assert(!heat.IsOverheated());
    assert(heat.GetHeatRatio() < 1.0f);
    heat.AddShotHeat();
    FillHeat(heat);
    heat.Update(0.4f, true);
    assert(!heat.IsOverheated());
    heat.Update(0.11f, true);
    assert(heat.IsOverheated());
}

void TestPausedTimeAndShotHeat()
{
    PlayerWeaponHeatTestAccess heat;
    heat.AddShotHeat();
    assert(heat.GetHeatRatio() > 0.0f && heat.GetHeatRatio() < 1.0f);
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
    PlayerWeaponHeatTestAccess heat;
    assert(heat.GetShotDamage(3) == 3);
    assert(heat.GetShotDamage(1) == 1);
    int previousDamage = 3;
    constexpr int kMaxShots = 1000;
    for (int shot = 0; shot < kMaxShots && heat.GetHeatRatio() < 1.0f; ++shot) {
        heat.AddShotHeat();
        const int damage = heat.GetShotDamage(3);
        assert(damage >= previousDamage && damage <= 6);
        previousDamage = damage;
    }
    assert(heat.GetShotDamage(3) == 6);
    assert(heat.GetShotDamage(1) == 2);
    heat.Update(1.0f, false);
    const float firstHeatRatio = heat.GetHeatRatio();
    const int firstDamage = heat.GetShotDamage(3);
    assert(firstHeatRatio > 0.0f && firstHeatRatio < 1.0f);
    assert(firstDamage >= 3 && firstDamage <= 6);
    heat.Update(1.0f, false);
    assert(heat.GetHeatRatio() < firstHeatRatio);
    assert(heat.GetShotDamage(3) <= firstDamage);
    heat.Update(100.0f, false);
    assert(heat.GetShotDamage(3) == 3);
    assert(heat.GetShotDamage(1) == 1);
}

void TestAssaultSustainedFireCanOverheat()
{
    PlayerWeaponHeatTestAccess heat;
    for (int shot = 0; shot < 100; ++shot) {
        heat.Update(0.05f, true);
        if (!heat.IsOverheated()) heat.AddShotHeat();
        else break;
    }
    assert(heat.IsOverheated());
}

void TestFiringStopsCooling()
{
    for (int framesPerSecond : { 30, 60, 120 }) {
        PlayerWeaponHeatTestAccess heat;
        for (int shotIndex = 0; shotIndex < 10; ++shotIndex) { heat.AddShotHeat(); }
        const float heatBeforeHold = heat.GetHeatRatio();
        assert(heatBeforeHold > 0.0f && heatBeforeHold < 1.0f);
        const float deltaTimeSeconds = 1.0f / static_cast<float>(framesPerSecond);
        for (int frame = 0; frame < framesPerSecond; ++frame) {
            heat.Update(deltaTimeSeconds, true);
            assert(heat.GetHeatRatio() == heatBeforeHold);
        }
        heat.AddShotHeat();
        assert(heat.GetHeatRatio() > heatBeforeHold);
        const float heatBeforeRelease = heat.GetHeatRatio();
        heat.Update(deltaTimeSeconds, false);
        assert(heat.GetHeatRatio() < heatBeforeRelease);
        const float heatAfterRelease = heat.GetHeatRatio();
        heat.Update(1.0f, true);
        assert(heat.GetHeatRatio() == heatAfterRelease);
    }
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
    TestFiringStopsCooling();
}

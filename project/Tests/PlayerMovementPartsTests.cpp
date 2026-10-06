#include "App/Game/Player/Movement/PlayerRollController.h"
#include "App/Game/Player/Movement/PlayerSteeringController.h"
#include <cassert>
#include <cmath>

namespace {
bool IsNear(float actual, float expected)
{
    return std::abs(actual - expected) < 0.0001f;
}

void TestRollDurationAndCooldown()
{
    PlayerRollController roll;
    EulerTransform transform{};
    roll.Update(transform, true, false);
    assert(!roll.IsRolling());
    roll.Update(transform, true, false);
    assert(roll.IsRolling());
    roll.Update(transform, false, false);
    assert(transform.rotate.z > 0.0f);
    for (int frameIndex = 0; frameIndex < 28; ++frameIndex) {
        roll.Update(transform, false, false);
    }
    assert(roll.IsRolling());
    roll.Update(transform, false, false);
    assert(!roll.IsRolling());
    assert(transform.rotate.z == 0.0f);
    roll.Update(transform, false, true);
    roll.Update(transform, false, true);
    assert(!roll.IsRolling());
    for (int frameIndex = 0; frameIndex < 58; ++frameIndex) {
        roll.Update(transform, false, false);
    }
    roll.Update(transform, false, true);
    roll.Update(transform, false, true);
    assert(roll.IsRolling());
    roll.Update(transform, false, false);
    assert(transform.rotate.z < 0.0f);
    roll.Stop();
    assert(!roll.IsRolling());
}

void TestRollTapExpiry()
{
    PlayerRollController roll;
    EulerTransform transform{};
    roll.Update(transform, true, false);
    for (int frameIndex = 0; frameIndex < 15; ++frameIndex) {
        roll.Update(transform, false, false);
    }
    roll.Update(transform, true, false);
    assert(!roll.IsRolling());
}

void TestSteeringDeadZoneAndReset()
{
    PlayerSteeringController steering;
    const Vector2 screenSizePixels{ 1280.0f, 720.0f };
    steering.Update({ 650.0f, 360.0f }, screenSizePixels, false, 1.0f / 60.0f);
    assert(steering.GetSteeringInput().x == 0.0f);
    steering.Update({ 1280.0f, 0.0f }, screenSizePixels, false, 1.0f / 60.0f);
    assert(IsNear(steering.GetSteeringInput().x, 0.20f));
    assert(IsNear(steering.GetSteeringInput().y, 0.20f));
    steering.Update({ 640.0f, 360.0f }, { 0.0f, 720.0f }, false, 1.0f / 60.0f);
    assert(IsNear(steering.GetSteeringInput().x, 0.20f));
    steering.Reset();
    assert(steering.GetSteeringInput().x == 0.0f);
    assert(steering.GetSteeringInput().y == 0.0f);
}

void TestAllRangeSteeringTimeStep()
{
    PlayerSteeringController fullStep;
    PlayerSteeringController halfStep;
    const Vector2 aimPosition{ 1280.0f, 360.0f };
    const Vector2 screenSizePixels{ 1280.0f, 720.0f };
    fullStep.Update(aimPosition, screenSizePixels, true, 1.0f / 60.0f);
    halfStep.Update(aimPosition, screenSizePixels, true, 1.0f / 120.0f);
    halfStep.Update(aimPosition, screenSizePixels, true, 1.0f / 120.0f);
    assert(IsNear(fullStep.GetSteeringInput().x, halfStep.GetSteeringInput().x));
    const float previousInput = fullStep.GetSteeringInput().x;
    fullStep.Update(aimPosition, screenSizePixels, true, 0.0f);
    assert(fullStep.GetSteeringInput().x == previousInput);
    fullStep.SetMouseSensitivity(0.0f);
    assert(fullStep.GetMouseSensitivity() == 0.5f);
    fullStep.SetMouseSensitivity(100.0f);
    assert(fullStep.GetMouseSensitivity() == 2.0f);
}
}

int main()
{
    TestRollDurationAndCooldown();
    TestRollTapExpiry();
    TestSteeringDeadZoneAndReset();
    TestAllRangeSteeringTimeStep();
}

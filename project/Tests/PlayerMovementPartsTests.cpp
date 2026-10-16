#include "Tests/PlayerTestAccess.h"
#include <cassert>
#include <cmath>

namespace {
bool IsNear(float actual, float expected)
{
    return std::abs(actual - expected) < 0.0001f;
}

void TestRollDurationAndCooldown()
{
    Player roll;
    EulerTransform transform{};
    PlayerTestAccess::UpdateRoll(roll, transform, true, false);
    assert(!roll.IsRolling());
    PlayerTestAccess::UpdateRoll(roll, transform, true, false);
    assert(roll.IsRolling());
    PlayerTestAccess::UpdateRoll(roll, transform, false, false);
    assert(transform.rotate.z > 0.0f);
    for (int frameIndex = 0; frameIndex < 28; ++frameIndex) {
        PlayerTestAccess::UpdateRoll(roll, transform, false, false);
    }
    assert(roll.IsRolling());
    PlayerTestAccess::UpdateRoll(roll, transform, false, false);
    assert(!roll.IsRolling());
    assert(transform.rotate.z == 0.0f);
    PlayerTestAccess::UpdateRoll(roll, transform, false, true);
    PlayerTestAccess::UpdateRoll(roll, transform, false, true);
    assert(!roll.IsRolling());
    for (int frameIndex = 0; frameIndex < 58; ++frameIndex) {
        PlayerTestAccess::UpdateRoll(roll, transform, false, false);
    }
    PlayerTestAccess::UpdateRoll(roll, transform, false, true);
    PlayerTestAccess::UpdateRoll(roll, transform, false, true);
    assert(roll.IsRolling());
    PlayerTestAccess::UpdateRoll(roll, transform, false, false);
    assert(transform.rotate.z < 0.0f);
    PlayerTestAccess::StopRoll(roll);
    assert(!roll.IsRolling());
}

void TestRollTapExpiry()
{
    Player roll;
    EulerTransform transform{};
    PlayerTestAccess::UpdateRoll(roll, transform, true, false);
    for (int frameIndex = 0; frameIndex < 15; ++frameIndex) {
        PlayerTestAccess::UpdateRoll(roll, transform, false, false);
    }
    PlayerTestAccess::UpdateRoll(roll, transform, true, false);
    assert(!roll.IsRolling());
}

void TestRollAtDifferentFrameRates()
{
    const int frameRates[] = { 30, 120 };
    for (int framesPerSecond : frameRates) {
        Player roll;
        EulerTransform transform{};
        const float deltaTimeSeconds = 1.0f / static_cast<float>(framesPerSecond);
        PlayerTestAccess::UpdateRoll(roll, transform, true, false, deltaTimeSeconds);
        PlayerTestAccess::UpdateRoll(roll, transform, true, false, deltaTimeSeconds);
        assert(roll.IsRolling());
        const int durationFrames = framesPerSecond / 2;
        for (int frameIndex = 0; frameIndex < durationFrames - 1; ++frameIndex) {
            PlayerTestAccess::UpdateRoll(roll, transform, false, false, deltaTimeSeconds);
        }
        assert(roll.IsRolling());
        PlayerTestAccess::UpdateRoll(roll, transform, false, false, deltaTimeSeconds);
        assert(!roll.IsRolling());
        assert(transform.rotate.z == 0.0f);
    }
}

void TestSteeringDeadZoneAndReset()
{
    Player steering;
    const Vector2 screenSizePixels{ 1280.0f, 720.0f };
    PlayerTestAccess::UpdateSteering(steering, { 650.0f, 360.0f }, screenSizePixels, false, 1.0f / 60.0f);
    assert(steering.GetStarFoxSteeringInput().x == 0.0f);
    PlayerTestAccess::UpdateSteering(steering, { 1280.0f, 0.0f }, screenSizePixels, false, 1.0f / 60.0f);
    assert(IsNear(steering.GetStarFoxSteeringInput().x, 1.0f));
    assert(IsNear(steering.GetStarFoxSteeringInput().y, 1.0f));
    PlayerTestAccess::UpdateSteering(steering, { 640.0f, 360.0f }, { 0.0f, 720.0f }, false, 1.0f / 60.0f);
    assert(IsNear(steering.GetStarFoxSteeringInput().x, 1.0f));
    PlayerTestAccess::ResetSteering(steering);
    assert(steering.GetStarFoxSteeringInput().x == 0.0f);
    assert(steering.GetStarFoxSteeringInput().y == 0.0f);
}

void TestAllRangeSteeringTimeStep()
{
    Player fullStep;
    Player halfStep;
    const Vector2 aimPosition{ 1280.0f, 360.0f };
    const Vector2 screenSizePixels{ 1280.0f, 720.0f };
    PlayerTestAccess::UpdateSteering(fullStep, aimPosition, screenSizePixels, true, 1.0f / 60.0f);
    PlayerTestAccess::UpdateSteering(halfStep, aimPosition, screenSizePixels, true, 1.0f / 120.0f);
    PlayerTestAccess::UpdateSteering(halfStep, aimPosition, screenSizePixels, true, 1.0f / 120.0f);
    assert(IsNear(fullStep.GetStarFoxSteeringInput().x, halfStep.GetStarFoxSteeringInput().x));
    const float previousInput = fullStep.GetStarFoxSteeringInput().x;
    PlayerTestAccess::UpdateSteering(fullStep, aimPosition, screenSizePixels, true, 0.0f);
    assert(fullStep.GetStarFoxSteeringInput().x == previousInput);
    fullStep.SetMouseSensitivity(0.0f);
    assert(fullStep.GetMouseSensitivity() == 0.5f);
    fullStep.SetMouseSensitivity(100.0f);
    assert(fullStep.GetMouseSensitivity() == 2.0f);
}

void TestGamepadSteeringIndependentOfMouse()
{
    const bool modes[] = { false, true };
    for (bool allRange : modes) {
        Player padLowMouse;
        Player padHighMouse;
        padLowMouse.SetMouseSensitivity(0.5f);
        padHighMouse.SetMouseSensitivity(2.0f);
        const Vector2 aim{ 1120.0f, 90.0f };
        const Vector2 size{ 1280.0f, 720.0f };
        PlayerTestAccess::UpdateSteering(padLowMouse, aim, size, allRange, 1.0f / 60.0f, true);
        PlayerTestAccess::UpdateSteering(padHighMouse, aim, size, allRange, 1.0f / 60.0f, true);
        assert(IsNear(padLowMouse.GetStarFoxSteeringInput().x, 0.75f));
        assert(IsNear(padLowMouse.GetStarFoxSteeringInput().y, 0.75f));
        assert(IsNear(padLowMouse.GetStarFoxSteeringInput().x, padHighMouse.GetStarFoxSteeringInput().x));
        assert(IsNear(padLowMouse.GetStarFoxSteeringInput().y, padHighMouse.GetStarFoxSteeringInput().y));
        PlayerTestAccess::ResetSteering(padLowMouse);
        PlayerTestAccess::UpdateSteering(padLowMouse, { 644.0f, 356.0f }, size, allRange, 1.0f / 60.0f, true);
        assert(padLowMouse.GetStarFoxSteeringInput().x > 0.0f);
        assert(padLowMouse.GetStarFoxSteeringInput().y > 0.0f);
    }
}

void TestGamepadImmediateFollowAndReversal()
{
    const bool modes[] = { false, true };
    const int frameRates[] = { 30, 60, 120 };
    for (bool allRange : modes) {
        for (int framesPerSecond : frameRates) {
            Player steering;
            const float dt = 1.0f / static_cast<float>(framesPerSecond);
            const Vector2 size{ 1280.0f, 720.0f };
            PlayerTestAccess::UpdateSteering(steering, { 1120.0f, 90.0f }, size, allRange, dt, true);
            assert(IsNear(steering.GetStarFoxSteeringInput().x, 0.75f));
            assert(IsNear(steering.GetStarFoxSteeringInput().y, 0.75f));
            PlayerTestAccess::UpdateSteering(steering, { 160.0f, 630.0f }, size, allRange, dt, true);
            assert(IsNear(steering.GetStarFoxSteeringInput().x, -0.75f));
            assert(IsNear(steering.GetStarFoxSteeringInput().y, -0.75f));
            PlayerTestAccess::UpdateSteering(steering, { 640.0f, 360.0f }, size, allRange, dt, true);
            assert(steering.GetStarFoxSteeringInput().x == 0.0f);
            assert(steering.GetStarFoxSteeringInput().y == 0.0f);
        }
    }
}
}

int main()
{
    TestRollDurationAndCooldown();
    TestRollTapExpiry();
    TestRollAtDifferentFrameRates();
    TestSteeringDeadZoneAndReset();
    TestAllRangeSteeringTimeStep();
    TestGamepadSteeringIndependentOfMouse();
    TestGamepadImmediateFollowAndReversal();
}

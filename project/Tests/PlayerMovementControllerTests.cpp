#include "App/Game/Player/Movement/PlayerMovementController.h"
#include "Engine/Camera/Camera.h"
#include "Engine/Winapp/WinApp.h"
#include <cassert>
#include <cmath>

namespace {
bool IsNear(float actual, float expected)
{
    return std::abs(actual - expected) < 0.0001f;
}

void TestRailFrameAndAreaForce()
{
    EulerTransform transform{};
    transform.translate = { 10.0f, 20.0f, 30.0f };
    PlayerAimController aim;
    PlayerMovementController movement(transform, aim);
    movement.Initialize();
    movement.SetRailFrame({ 10.0f, 20.0f, 30.0f }, { 0.0f, 0.0f, 1.0f },
        { 0.0f, 1.0f, 0.0f }, { -1.0f, 0.0f, 0.0f });
    movement.ApplyRailAreaForce({ 100.0f, -100.0f, 0.0f });
    assert(movement.GetRailOffset().x == 20.0f);
    assert(movement.GetRailOffset().y == -12.0f);
    movement.ApplyRailPosition();
    assert(transform.translate.x == 10.0f);
    assert(transform.translate.y == 8.0f);
    assert(transform.translate.z == 50.0f);
    assert(movement.GetAutomaticWorldVelocity().x == -0.5f);
}

void TestAllRangeAndControls()
{
    EulerTransform transform{};
    transform.translate = { 12.0f, 34.0f, 56.0f };
    transform.rotate = { 0.2f, 0.4f, 0.0f };
    PlayerAimController aim;
    PlayerMovementController movement(transform, aim);
    movement.Initialize();
    movement.ApplyRailAreaForce({ 3.0f, 4.0f, 0.0f });
    movement.EnableAllRangeMode(600.0f, 5.0f, 180.0f);
    assert(movement.IsAllRangeMode());
    assert(!movement.IsReturningToFlightArea());
    assert(movement.GetRailOffset().x == 0.0f);
    assert(movement.GetRailOffset().y == 0.0f);
    const Vector3 forward = movement.GetFlightForward();
    assert(IsNear(forward.x, std::sin(-0.4f) * std::cos(-0.2f)));
    assert(IsNear(forward.y, std::sin(-0.2f)));
    assert(IsNear(forward.z, std::cos(-0.4f) * std::cos(-0.2f)));
    movement.ApplyRailPosition();
    assert(transform.translate.x == 12.0f);
    assert(transform.translate.y == 34.0f);
    assert(transform.translate.z == 56.0f);
    movement.SetMouseSensitivity(0.0f);
    assert(movement.GetMouseSensitivity() == 0.5f);
    movement.SetMouseSensitivity(10.0f);
    assert(movement.GetMouseSensitivity() == 2.0f);
    movement.SetControlMode(PlayerMovementController::ControlMode::KeyboardAndMouse);
    assert(movement.GetControlMode() == PlayerMovementController::ControlMode::KeyboardAndMouse);
}

void TestScreenConstraintWithCamera()
{
    Camera camera;
    camera.SetProjectionJitter({ 0.0f, 0.0f });
    Vector3 basePosition{};
    const Vector3 right{ 1.0f, 0.0f, 0.0f };
    const Vector3 up{ 0.0f, 1.0f, 0.0f };
    const Vector3 forward{ 0.0f, 0.0f, 1.0f };
    PlayerScreenConstraint constraint(basePosition, right, up, forward);
    constraint.SetCamera(&camera);
    const Vector3 correctedOffset = constraint.ClampRailOffsetToScreen({ 20.0f, 12.0f, 99.0f });
    assert(correctedOffset.z == 0.0f);
    const Vector3 worldPosition = constraint.CalculateRailWorldPosition(correctedOffset);
    const float screenWidth = static_cast<float>(WinApp::GetInstance()->GetClientWidth());
    const float screenHeight = static_cast<float>(WinApp::GetInstance()->GetClientHeight());
    for (int cornerIndex = 0; cornerIndex < 4; ++cornerIndex) {
        float cornerX = -1.5f;
        float cornerY = -1.0f;
        if (cornerIndex % 2 == 1) {
            cornerX = 1.5f;
        }
        if (cornerIndex >= 2) {
            cornerY = 1.0f;
        }
        const Vector2 screenPosition = camera.WorldToScreen(worldPosition + Vector3{ cornerX, cornerY, 0.0f });
        assert(screenPosition.x >= 99.0f);
        assert(screenPosition.x <= screenWidth - 99.0f);
        assert(screenPosition.y >= 99.0f);
        assert(screenPosition.y <= screenHeight - 99.0f);
    }
    basePosition = { 0.0f, 0.0f, 50.0f };
    const Vector3 updatedWorldPosition = constraint.CalculateRailWorldPosition({ 1.0f, 2.0f, 0.0f });
    assert(updatedWorldPosition.z == 50.0f);
}
}

int main()
{
    TestRailFrameAndAreaForce();
    TestAllRangeAndControls();
    TestScreenConstraintWithCamera();
}



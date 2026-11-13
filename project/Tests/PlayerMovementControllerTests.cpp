#include "Tests/PlayerTestAccess.h"
#include "Engine/Camera/Camera.h"
#include "Engine/Winapp/WinApp.h"
#include "Engine/input/Input.h"
#include "Engine/Time/TimeManager.h"
#include <chrono>
#include <thread>
#include <cassert>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <limits>

// 実機なしでも、同時押し・切断・接続直後の状態を再現する。
struct InputTestAccess {
    static void SetPitchInverted(Input& input, bool inverted) { input.gamepadPitchInverted_ = inverted; }

    static void SetKeyPressed(Input& input, BYTE keyCode, bool isPressed)
    {
        input.keys_[keyCode] = 0;
        if (isPressed) {
            input.keys_[keyCode] = 0x80;
        }
    }

    static void SetMousePressed(Input& input, int button, bool isPressed)
    {
        input.mouseState_.rgbButtons[button] = 0;
        if (isPressed) {
            input.mouseState_.rgbButtons[button] = 0x80;
        }
    }

    static void Set(Input& input, XINPUT_GAMEPAD current, XINPUT_GAMEPAD previous = {},
        bool connected = true, bool ready = true, bool keyboardFire = false) {
        input.gamepadState_.Gamepad = current;
        input.preGamepadState_.Gamepad = previous;
        input.isGamepadConnected_ = connected;
        input.gamepadButtonsReady_ = ready;
        input.isUsingGamepad_ = connected;
        input.keys_[DIK_SPACE] = keyboardFire ? 0x80 : 0;
        input.preKeys_[DIK_SPACE] = 0;
    }
};

namespace {
void TestGamepadFireAndDeadZone()
{
    Input& input = *Input::GetInstance();
    assert(!input.IsGamepadPitchInverted());
    XINPUT_GAMEPAD pad{};
    pad.bRightTrigger = XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
    pad.sThumbLX = XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE;
    InputTestAccess::Set(input, pad);
    assert(!input.IsGamepadRightTriggerPressed());
    assert(input.GetGamepadLeftStickX() == 0.0f);
    pad.bRightTrigger++;
    pad.sThumbLX = 32767;
    InputTestAccess::Set(input, pad);
    assert(input.IsGamepadRightTriggerPressed());
    assert(std::abs(input.GetGamepadLeftStickX() - 1.0f) < 0.0001f);
    pad.sThumbLY = 32767;
    InputTestAccess::Set(input, pad);
    const float x = input.GetGamepadLeftStickX();
    const float y = input.GetGamepadLeftStickY();
    assert(std::abs(x * x + y * y - 1.0f) < 0.0001f);
    XINPUT_GAMEPAD previous = pad;
    pad.wButtons = XINPUT_GAMEPAD_A;
    InputTestAccess::Set(input, pad, previous);
    assert(input.IsGamepadButtonPressed(XINPUT_GAMEPAD_A));
    pad.bRightTrigger = 0;
    InputTestAccess::Set(input, pad, previous);
    assert(input.IsGamepadButtonPressed(XINPUT_GAMEPAD_A));
    InputTestAccess::Set(input, pad, {}, true, false);
    assert(!input.IsGamepadButtonTrigger(XINPUT_GAMEPAD_A));
    assert(!input.IsGamepadRightTriggerPressed()); // 再接続時の押しっぱなしを無視する。
    InputTestAccess::Set(input, pad, {}, false);
    assert(!input.IsGamepadRightTriggerPressed());
    assert(input.GetGamepadLeftStickX() == 0.0f);
    InputTestAccess::Set(input, {}, {}, false, false, true);
    assert(input.IsKeyPressed(DIK_SPACE)); // パッドなしでもSpace入力が使える。
    InputTestAccess::Set(input, {});
}

void TestGamepadAimAndRailControls()
{
    Input& input = *Input::GetInstance();
    EulerTransform transform{};
    Player movement;
    PlayerTestAccess::Transform(movement) = transform;
    PlayerTestAccess::InitializeMovement(movement);
    movement.SetControlMode(Player::ControlMode::KeyboardAndMouse);
    XINPUT_GAMEPAD pad{};
    pad.wButtons = XINPUT_GAMEPAD_LEFT_SHOULDER;
    pad.sThumbLX = 32767;
    InputTestAccess::Set(input, pad);
    PlayerTestAccess::UpdateMovement(movement, &input);
    assert(movement.GetRailOffset().x > 0.0f); // WASDモードでもパッドのスティックで機体を直接操舵する。
    InputTestAccess::Set(input, {}, pad);
    PlayerTestAccess::UpdateMovement(movement, &input);
    InputTestAccess::Set(input, pad);
    PlayerTestAccess::UpdateMovement(movement, &input);
    InputTestAccess::Set(input, pad, pad);
    PlayerTestAccess::UpdateMovement(movement, &input);
    assert(std::abs(PlayerTestAccess::Transform(movement).rotate.z) < 0.0001f);
    PlayerTestAccess::StopMovement(movement);
    pad.wButtons = XINPUT_GAMEPAD_X | XINPUT_GAMEPAD_Y;
    InputTestAccess::Set(input, pad);
    PlayerTestAccess::UpdateBoost(movement, &input, false);
    assert(!movement.IsBoosting());
    assert(movement.GetForwardSpeedMultiplier() == 0.5f);
    PlayerTestAccess::UpdateBoost(movement, &input, true);
    assert(movement.GetForwardSpeedMultiplier() == 1.0f);
    InputTestAccess::Set(input, {});
}

bool IsNear(float actual, float expected)
{
    return std::abs(actual - expected) < 0.0001f;
}

// パッド照準は時間刻みによらず追従し、入力を離すと正面へ戻る。
void TestDirectGamepadAim()
{
    Input& input = *Input::GetInstance();
    XINPUT_GAMEPAD pad{};
    pad.sThumbLX = 32767;
    InputTestAccess::Set(input, pad);
    PlayerReticleController coarse, fine;
    for (int i = 0; i < 30; ++i) { coarse.UpdateDirectGamepadControl(input, 1.0f / 30.0f); }
    for (int i = 0; i < 120; ++i) { fine.UpdateDirectGamepadControl(input, 1.0f / 120.0f); }
    assert(IsNear(coarse.GetDirectGamepadAimInput().x, fine.GetDirectGamepadAimInput().x));
    assert(coarse.GetDirectGamepadAimInput().x > 0.9f);
    InputTestAccess::Set(input, {});
    for (int i = 0; i < 120; ++i) { coarse.UpdateDirectGamepadControl(input, 1.0f / 60.0f); }
    assert(std::abs(coarse.GetDirectGamepadAimInput().x) < 0.001f);
    pad = {};
    pad.sThumbLY = 32767;
    InputTestAccess::Set(input, pad);
    coarse.ResetDirectGamepadAim();
    const Vector2 normalSteering = coarse.UpdateDirectGamepadControl(input, 1.0f);
    InputTestAccess::SetPitchInverted(input, true);
    fine.ResetDirectGamepadAim();
    const Vector2 invertedSteering = fine.UpdateDirectGamepadControl(input, 1.0f);
    assert(IsNear(normalSteering.y, -invertedSteering.y));
    assert(IsNear(coarse.GetDirectGamepadAimInput().y, -fine.GetDirectGamepadAimInput().y));
    const Vector3 forward { 0.0f, 0.0f, 1.0f }, right { 1.0f, 0.0f, 0.0f }, up { 0.0f, 1.0f, 0.0f };
    assert(coarse.GetDirectGamepadAimDirection(false, forward, right, up).y > 0.0f);
    const Vector3 allRange = coarse.GetDirectGamepadAimDirection(true, forward, right, up);
    assert(IsNear(allRange.x, forward.x) && IsNear(allRange.y, forward.y) && IsNear(allRange.z, forward.z));
    Camera camera;
    coarse.UpdateDirectGamepadAim(camera, {}, false, forward, right, up, 1000.0f);
    const Vector2 screen = coarse.GetAimScreenPosition();
    assert(std::isfinite(screen.x) && std::isfinite(screen.y));
    assert(screen.x >= 64.0f && screen.x <= WinApp::kClientWidth - 64.0f);
    assert(screen.y >= 64.0f && screen.y <= WinApp::kClientHeight - 64.0f);
    InputTestAccess::SetPitchInverted(input, false);
    InputTestAccess::Set(input, {});
}

void TestRailFrameAndAreaForce()
{
    EulerTransform transform{};
    transform.translate = { 10.0f, 20.0f, 30.0f };
    Player aim;
    Player movement;
    PlayerTestAccess::Transform(movement) = transform;
    PlayerTestAccess::InitializeMovement(movement);
    movement.SetRailFrame({ 10.0f, 20.0f, 30.0f }, { 0.0f, 0.0f, 1.0f },
        { 0.0f, 1.0f, 0.0f }, { -1.0f, 0.0f, 0.0f });
    movement.ApplyRailAreaForce({ 100.0f, -100.0f, 0.0f });
    assert(movement.GetRailOffset().x == 20.0f);
    assert(movement.GetRailOffset().y == -15.0f);
    PlayerTestAccess::ApplyRailPosition(movement);
    assert(PlayerTestAccess::Transform(movement).translate.x == 10.0f);
    assert(PlayerTestAccess::Transform(movement).translate.y == 5.0f);
    assert(PlayerTestAccess::Transform(movement).translate.z == 50.0f);
    assert(movement.GetAutomaticWorldVelocity().x == -0.5f);
    PlayerTestAccess::Transform(movement).translate = { 4.0f, 5.0f, 6.0f };
    PlayerTestAccess::SynchronizePosition(movement);
    PlayerTestAccess::ApplyRailPosition(movement);
    assert(PlayerTestAccess::Transform(movement).translate.x == 4.0f);
    assert(PlayerTestAccess::Transform(movement).translate.y == 5.0f);
    assert(PlayerTestAccess::Transform(movement).translate.z == 6.0f);
}

void TestAllRangeAndControls()
{
    EulerTransform transform{};
    transform.translate = { 12.0f, 34.0f, 56.0f };
    transform.rotate = { 0.2f, 0.4f, 0.0f };
    Player aim;
    Player movement;
    PlayerTestAccess::Transform(movement) = transform;
    PlayerTestAccess::InitializeMovement(movement);
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
    PlayerTestAccess::ApplyRailPosition(movement);
    assert(PlayerTestAccess::Transform(movement).translate.x == 12.0f);
    assert(PlayerTestAccess::Transform(movement).translate.y == 34.0f);
    assert(PlayerTestAccess::Transform(movement).translate.z == 56.0f);
    movement.SetMouseSensitivity(0.0f);
    assert(movement.GetMouseSensitivity() == 0.5f);
    movement.SetMouseSensitivity(10.0f);
    assert(movement.GetMouseSensitivity() == 2.0f);
    movement.SetControlMode(Player::ControlMode::KeyboardAndMouse);
    assert(movement.GetControlMode() == Player::ControlMode::KeyboardAndMouse);
    PlayerTestAccess::Transform(movement).rotate = { 0.3f, -0.5f, 0.0f };
    PlayerTestAccess::SynchronizeOrientation(movement);
    const Vector3 synchronizedForward = movement.GetFlightForward();
    assert(IsNear(synchronizedForward.x, std::sin(0.5f) * std::cos(-0.3f)));
    assert(IsNear(synchronizedForward.y, std::sin(-0.3f)));
}

void TestScreenConstraintWithCamera()
{
    Camera camera;
    camera.SetProjectionJitter({ 0.0f, 0.0f });
    Vector3 basePosition{};
    const Vector3 right{ 1.0f, 0.0f, 0.0f };
    const Vector3 up{ 0.0f, 1.0f, 0.0f };
    const Vector3 forward{ 0.0f, 0.0f, 1.0f };
    Player constraint;
    constraint.SetRailFrame(basePosition, right, up, forward);
    constraint.SetCamera(&camera);
    const Vector3 correctedOffset = PlayerTestAccess::ClampRailOffsetToScreen(constraint, { 20.0f, 12.0f, 99.0f });
    assert(correctedOffset.z == 0.0f);
    const Vector3 worldPosition = PlayerTestAccess::CalculateRailWorldPosition(constraint, correctedOffset);
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
    constraint.SetRailFrame(basePosition, right, up, forward);
    const Vector3 updatedWorldPosition = PlayerTestAccess::CalculateRailWorldPosition(constraint, { 1.0f, 2.0f, 0.0f });
    assert(updatedWorldPosition.z == 50.0f);
}
}

// PlayerやStateを介さず、渡した経過時間だけで飛行を進められる。
void TestIndependentAllRangeMovement()
{
    const int frameRates[] = { 30, 60, 120 };
    for (int framesPerSecond : frameRates) {
        PlayerMovement movement;
        EulerTransform transform{};
        transform.translate = { 0.0f, 50.0f, 0.0f };
        movement.InitializeMovement(transform);
        movement.EnableAllRangeMode(transform, 600.0f, 5.0f, 180.0f);
        const float deltaTimeSeconds = 1.0f / static_cast<float>(framesPerSecond);
        for (int frameIndex = 0; frameIndex < framesPerSecond; ++frameIndex) {
            movement.UpdateMovement(transform, {}, 1.0f, deltaTimeSeconds);
        }
        assert(IsNear(transform.translate.z, 30.0f));
        assert(IsNear(transform.translate.y, 50.0f));

        assert(movement.UpdateBoost(false, true));
        movement.UpdateMovement(transform, {}, 1.0f, 1.0f);
        assert(IsNear(transform.translate.z, 90.0f));
        assert(!movement.UpdateBoost(true, true));
        assert(!movement.IsBoosting());
        movement.UpdateMovement(transform, {}, 1.0f, 1.0f);
        assert(IsNear(transform.translate.z, 105.0f));
    }
}

void TestMovementWithoutBank()
{
    for (int framesPerSecond : { 30, 60, 120 }) {
        for (float direction : { -1.0f, 1.0f }) {
            PlayerMovement movement;
            EulerTransform transform {};
            transform.translate = { 0.0f, 50.0f, 0.0f };
            transform.rotate.z = 0.4f;
            movement.InitializeMovement(transform);
            movement.EnableAllRangeMode(transform, 600.0f, 5.0f, 180.0f);
            const float deltaTimeSeconds = 1.0f / static_cast<float>(framesPerSecond);
            for (int frame = 0; frame < framesPerSecond; ++frame) {
                movement.UpdateMovement(transform, { direction, 0.0f }, 1.0f, deltaTimeSeconds);
                assert(IsNear(transform.rotate.z, 0.0f));
            }
        }
    }
}

// 外力による下降でも、レール上の最低高度を維持する。
void TestMovementRailMinimumHeight()
{
    PlayerMovement movement;
    EulerTransform transform{};
    transform.translate = { 0.0f, 2.0f, 0.0f };
    movement.InitializeMovement(transform);
    movement.SetRailMinimumHeight(5.0f);
    movement.ApplyRailAreaForce({ 0.0f, -100.0f, 0.0f }, 1.0f / 60.0f);
    movement.ApplyRailPosition(transform);
    assert(IsNear(transform.translate.y, 5.0f));
}

// 設定した上下限は従来の上下15を越えても有効で、外力にも適用される。
void TestMovementRailHeightRange()
{
    PlayerMovement movement;
    EulerTransform transform {};
    transform.translate = { 0.0f, -85.0f, 0.0f };
    movement.InitializeMovement(transform);
    movement.SetRailHeightRange({ -110.0f, -55.0f });
    movement.ApplyRailAreaForce({ 0.0f, -100.0f, 0.0f }, 1.0f / 60.0f);
    movement.ApplyRailPosition(transform);
    assert(IsNear(transform.translate.y, -110.0f));
    movement.ApplyRailAreaForce({ 0.0f, 200.0f, 0.0f }, 1.0f / 60.0f);
    movement.ApplyRailPosition(transform);
    assert(IsNear(transform.translate.y, -55.0f));
    movement.SetRailHeightRange({ -92.0f, -70.0f });
    movement.ApplyRailAreaForce({}, 1.0f / 60.0f);
    movement.ApplyRailPosition(transform);
    assert(IsNear(transform.translate.y, -70.0f));
}

void TestPlayerInputOperations()
{
    Input& input = *Input::GetInstance();
    InputTestAccess::Set(input, {}, {}, false);
    InputTestAccess::SetKeyPressed(input, DIK_W, true);
    InputTestAccess::SetKeyPressed(input, DIK_D, true);
    Vector2 steering = PlayerInput::ReadKeyboardSteering(input);
    assert(steering.x == 1.0f && steering.y == 1.0f);
    InputTestAccess::SetKeyPressed(input, DIK_A, true);
    steering = PlayerInput::ReadKeyboardSteering(input);
    assert(steering.x == 0.0f);
    InputTestAccess::SetKeyPressed(input, DIK_W, false);
    InputTestAccess::SetKeyPressed(input, DIK_D, false);
    InputTestAccess::SetKeyPressed(input, DIK_A, false);

    InputTestAccess::SetKeyPressed(input, DIK_LSHIFT, true);
    assert(PlayerInput::ReadBoost(input, true).shouldBoost);
    InputTestAccess::SetKeyPressed(input, DIK_LSHIFT, false);
    InputTestAccess::SetMousePressed(input, 1, true);
    assert(PlayerInput::ReadBoost(input, true).shouldBoost);
    InputTestAccess::SetMousePressed(input, 1, false);

    XINPUT_GAMEPAD pad{};
    pad.wButtons = XINPUT_GAMEPAD_LEFT_SHOULDER | XINPUT_GAMEPAD_X | XINPUT_GAMEPAD_Y;
    InputTestAccess::Set(input, pad);
    const PlayerInput::BoostInput boostInput = PlayerInput::ReadBoost(input, true);
    assert(boostInput.isBraking && boostInput.shouldBoost);
    PlayerMovement movement;
    movement.UpdateBoost(boostInput.isBraking, boostInput.shouldBoost);
    assert(!movement.IsBoosting());
    assert(IsNear(movement.GetForwardSpeedMultiplier(), 0.5f));
    const PlayerInput::BoostInput disabledBoost = PlayerInput::ReadBoost(input, false);
    assert(!disabledBoost.isBraking && !disabledBoost.shouldBoost);

    InputTestAccess::Set(input, {}, {}, false);
}

int main(int argumentCount, char** arguments)
{
    _set_error_mode(_OUT_TO_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    // 移動更新には実際の経過秒が必要。各テスト中は60FPS相当の値を維持する。
    TimeManager* timeManager = TimeManager::GetInstance();
    timeManager->Initialize();
    timeManager->SetMaxDeltaTime(0.001f);
    timeManager->SetTimeScale((1.0f / 60.0f) / 0.001f);
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    timeManager->Update();
    TestGamepadFireAndDeadZone();
    TestGamepadAimAndRailControls();
    TestDirectGamepadAim();
    TestRailFrameAndAreaForce();
    TestAllRangeAndControls();
    TestMovementWithoutBank();
    if (argumentCount == 2 && std::strcmp(arguments[1], "--movement-input-only") == 0) {
        TestIndependentAllRangeMovement();
        TestPlayerInputOperations();
        std::puts("PASS: movement input, flight without bank or roll, and frame rates");
        return 0;
    }
    TestScreenConstraintWithCamera();
    TestIndependentAllRangeMovement();
    TestMovementRailMinimumHeight();
    TestMovementRailHeightRange();
    TestPlayerInputOperations();
}



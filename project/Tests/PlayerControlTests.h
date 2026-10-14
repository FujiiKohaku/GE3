#pragma once
#include "App/Game/Player/Control/BasePlayerControl.h"
#include "App/Game/Player/Aim/PlayerAimController.h"
#include "App/Game/Player/Movement/PlayerMovementController.h"
#include "Engine/Input/Input.h"
#include "Engine/Winapp/WinApp.h"
#include <cmath>

struct InputTestAccess {
    static void Setup(Input& input, bool usingGamepad, bool right, bool up, SHORT stickX) {
        input.isUsingGamepad_ = usingGamepad;
        input.isGamepadConnected_ = usingGamepad;
        input.gamepadButtonsReady_ = true;
        input.gamepadState_.Gamepad = {};
        input.gamepadState_.Gamepad.sThumbLX = stickX;
        input.keys_[DIK_D] = right ? 0x80 : 0;
        input.keys_[DIK_W] = up ? 0x80 : 0;
        input.keys_[DIK_A] = 0;
        input.keys_[DIK_S] = 0;
        input.gamepadPitchInverted_ = false;
        input.gamepadAimSensitivity_ = 1.0f;
    }
};

inline bool RunPlayerControlTests()
{
    const auto close = [](float a, float b) { return std::abs(a - b) < 0.0001f; };
    Input& input = *Input::GetInstance();
    PlayerAimController aim;
    aim.Initialize();
    PlayerSteeringController steering;
    const Vector2 screen { static_cast<float>(WinApp::GetInstance()->GetClientWidth()),
        static_cast<float>(WinApp::GetInstance()->GetClientHeight()) };
    const BasePlayerControl& keyboard = GetPlayerControl(false, false);
    const BasePlayerControl& mouse = GetPlayerControl(false, true);
    const BasePlayerControl& gamepad = GetPlayerControl(true, false);
    if (&keyboard == &mouse || &mouse == &gamepad || &GetPlayerControl(true, true) != &gamepad) return false;
    InputTestAccess::Setup(input, false, true, true, 0);
    PlayerControlContext context { input, aim, steering, screen, false, 0.016f };
    auto result = keyboard.GetControl(context);
    if (result.steering.x != 1.0f || result.steering.y != 1.0f || result.railResponse != 1.0f) return false;
    context.allRange = true;
    result = keyboard.GetControl(context);
    if (result.steering.x != 1.0f || result.steering.y != 1.0f) return false;
    InputTestAccess::Setup(input, false, false, false, 0);
    aim.UpdateGamepadAim(1.0f, 0.0f, false, 0.1f);
    context.allRange = false;
    PlayerSteeringController expectedSteering;
    expectedSteering.Update(aim.GetAimScreenPosition(), screen, false, context.dt, false);
    result = mouse.GetControl(context);
    if (!close(result.steering.x, expectedSteering.GetSteeringInput().x) || result.railResponse != 2.75f) return false;
    aim.Initialize();
    InputTestAccess::Setup(input, true, false, false, 32767);
    gamepad.UpdateAim(aim, input, 0.1f);
    if (aim.GetAimScreenPosition().x <= screen.x * 0.5f) return false;
    expectedSteering.Update(aim.GetGamepadSteeringScreenPosition(), screen, false, context.dt, true);
    result = gamepad.GetControl(context);
    if (!close(result.steering.x, expectedSteering.GetSteeringInput().x) || result.railResponse != 2.75f) return false;
    steering.SetMouseSensitivity(2.0f);
    const auto sensitivityResult = gamepad.GetControl(context);
    if (!close(sensitivityResult.steering.x, result.steering.x)) return false;
    // 操作Strategyの選択は、設定より使用中のパッドを優先する。
    EulerTransform transform {};
    PlayerMovementController movement(transform, aim);
    movement.SetControlMode(PlayerMovementController::ControlMode::KeyboardAndMouse);
    if (&movement.GetControlStrategy(input) != &gamepad) return false;
    InputTestAccess::Setup(input, false, false, false, 0);
    if (&movement.GetControlStrategy(input) != &keyboard) return false;
    movement.SetControlMode(PlayerMovementController::ControlMode::StarFox);
    if (&movement.GetControlStrategy(input) != &mouse) return false;
    const Vector2 before = aim.GetAimScreenPosition();
    keyboard.ConstrainAim(aim, nullptr, {});
    gamepad.ConstrainAim(aim, nullptr, {});
    return aim.GetAimScreenPosition().x == before.x && aim.GetAimScreenPosition().y == before.y;
}

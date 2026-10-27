#include "PlayerInput.h"
#include "Engine/Input/Input.h"
#include "Engine/Winapp/WinApp.h"

PlayerInput::ControlInputType PlayerInput::GetControlInputType(const Input& input, bool shouldUseMouse)
{
    if (input.IsUsingGamepad()) {
        return ControlInputType::Gamepad;
    }
    if (shouldUseMouse) {
        return ControlInputType::Mouse;
    }
    return ControlInputType::Keyboard;
}

// デスクトップ座標を、ゲームの描画領域の左上を原点とする座標へ変換する。
Vector2 PlayerInput::ReadMousePosition()
{
    const HWND windowHandle = WinApp::GetInstance()->GetHwnd();
    POINT mousePosition = {};
    GetCursorPos(&mousePosition);
    ScreenToClient(windowHandle, &mousePosition);
    return { static_cast<float>(mousePosition.x), static_cast<float>(mousePosition.y) };
}

PlayerInput::AimInput PlayerInput::ReadGamepadAim(const Input& input)
{
    AimInput aimInput;
    aimInput.stick = { input.GetGamepadLeftStickX(), input.GetGamepadLeftStickY() };
    aimInput.isPitchInverted = input.IsGamepadPitchInverted();
    aimInput.sensitivity = input.GetGamepadAimSensitivity();
    return aimInput;
}

Vector2 PlayerInput::ReadKeyboardSteering(const Input& input)
{
    Vector2 steering = {};
    if (input.IsKeyPressed(DIK_A)) { steering.x -= 1.0f; }
    if (input.IsKeyPressed(DIK_D)) { steering.x += 1.0f; }
    if (input.IsKeyPressed(DIK_W)) { steering.y += 1.0f; }
    if (input.IsKeyPressed(DIK_S)) { steering.y -= 1.0f; }
    return steering;
}

PlayerInput::MovementInput PlayerInput::ReadMovement(const Input& input)
{
    MovementInput movementInput;
    if (input.IsGamepadButtonPressed(XINPUT_GAMEPAD_LEFT_SHOULDER)) { movementInput.manualBank += 0.9f; }
    if (input.IsGamepadButtonPressed(XINPUT_GAMEPAD_RIGHT_SHOULDER)) { movementInput.manualBank -= 0.9f; }
    movementInput.isLeftRollTriggered = input.IsGamepadButtonTrigger(XINPUT_GAMEPAD_LEFT_SHOULDER);
    movementInput.isRightRollTriggered = input.IsGamepadButtonTrigger(XINPUT_GAMEPAD_RIGHT_SHOULDER);
    return movementInput;
}

PlayerInput::BoostInput PlayerInput::ReadBoost(const Input& input, bool isEnabled)
{
    BoostInput boostInput;
    if (!isEnabled) {
        return boostInput;
    }
    boostInput.isBraking = input.IsKeyPressed(DIK_LCONTROL) || input.IsKeyPressed(DIK_RCONTROL) ||
        input.IsGamepadButtonPressed(XINPUT_GAMEPAD_Y);
    boostInput.shouldBoost = input.IsKeyPressed(DIK_LSHIFT) || input.IsMousePressed(1) ||
        input.IsGamepadButtonPressed(XINPUT_GAMEPAD_X);
    return boostInput;
}

#include "Input.h"
#include <cassert>
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "xinput.lib")

#include <algorithm>
#include <cmath>
#include <cstring>
std::unique_ptr<Input> Input::instance_ = nullptr;


Input* Input::GetInstance()
{
    if (!instance_) {
        instance_ = std::make_unique<Input>(ConstructorKey());
    }
    return instance_.get();
}

bool Input::Initialize(WinApp* winApp)
{
    HRESULT result;

    winApp_ = winApp;

    result = DirectInput8Create(
        winApp_->GetHinstance(),
        DIRECTINPUT_VERSION,
        IID_IDirectInput8,
        reinterpret_cast<void**>(directInput_.ReleaseAndGetAddressOf()),
        nullptr);
    assert(SUCCEEDED(result));

    result = directInput_->CreateDevice(
        GUID_SysKeyboard,
        keyboard_.ReleaseAndGetAddressOf(),
        nullptr);
    assert(SUCCEEDED(result));

    result = keyboard_->SetDataFormat(&c_dfDIKeyboard);
    assert(SUCCEEDED(result));

    result = keyboard_->SetCooperativeLevel(
        winApp_->GetHwnd(),
        DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
    assert(SUCCEEDED(result));

    result = directInput_->CreateDevice(
        GUID_SysMouse,
        mouse_.ReleaseAndGetAddressOf(),
        nullptr);
    assert(SUCCEEDED(result));

    result = mouse_->SetDataFormat(&c_dfDIMouse2);
    assert(SUCCEEDED(result));

    result = mouse_->SetCooperativeLevel(
        winApp_->GetHwnd(),
        DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
    assert(SUCCEEDED(result));

    return true;
}

void Input::Update()
{
    memcpy(preKeys_, keys_, sizeof(keys_));
    preMouseState_ = mouseState_;
    preGamepadState_ = gamepadState_;

    HRESULT result;

    result = keyboard_->Acquire();
    if (SUCCEEDED(result)) {
        result = keyboard_->GetDeviceState(sizeof(keys_), keys_);
        if (FAILED(result)) {
            keyboard_->Acquire();
        }
    }

    result = mouse_->Acquire();
    if (SUCCEEDED(result)) {
        result = mouse_->GetDeviceState(sizeof(mouseState_), &mouseState_);
        if (FAILED(result)) {
            mouse_->Acquire();
        }
    }

    XINPUT_STATE nextGamepadState = {};
    DWORD nextGamepadIndex = XUSER_MAX_COUNT;

    for (DWORD index = 0; index < XUSER_MAX_COUNT; ++index) {
        if (XInputGetState(index, &nextGamepadState) == ERROR_SUCCESS) {
            nextGamepadIndex = index;
            break;
        }
    }

    isGamepadConnected_ = nextGamepadIndex < XUSER_MAX_COUNT;
    if (!isGamepadConnected_) {
        gamepadState_ = {};
        gamepadIndex_ = XUSER_MAX_COUNT;
    } else {
        if (gamepadIndex_ != nextGamepadIndex) {
            preGamepadState_ = {};
        }
        gamepadState_ = nextGamepadState;
        gamepadIndex_ = nextGamepadIndex;
    }
}

bool Input::IsKeyPressed(BYTE keyCode) const
{
    return (keys_[keyCode] & 0x80) != 0;
}

bool Input::IsMousePressed(int button) const
{
    if (button < 0) {
        return false;
    }

    if (button >= 8) {
        return false;
    }

    return (mouseState_.rgbButtons[button] & 0x80) != 0;
}
bool Input::IsKeyTrigger(BYTE keyCode) const
{
    bool isNowPressed = (keys_[keyCode] & 0x80) != 0;

    bool wasPressed = (preKeys_[keyCode] & 0x80) != 0;

    return isNowPressed && !wasPressed;
}
bool Input::IsMouseTrigger(int button) const
{
    if (button < 0) {
        return false;
    }

    if (button >= 8) {
        return false;
    }

    bool isNowPressed = (mouseState_.rgbButtons[button] & 0x80) != 0;

    bool wasPressed = (preMouseState_.rgbButtons[button] & 0x80) != 0;

    return isNowPressed && !wasPressed;
}
LONG Input::GetMouseDeltaX() const
{
    return mouseState_.lX;
}

LONG Input::GetMouseDeltaY() const
{
    return mouseState_.lY;
}

LONG Input::GetMouseWheel() const
{
    return mouseState_.lZ;
}

void Input::Finalize()
{
    if (!instance_) {
        return;
    }

    instance_->mouse_.Reset();
    instance_->keyboard_.Reset();
    instance_->directInput_.Reset();
    instance_.reset();
}
void Input::ResetMouseDelta()
{
    mouseState_.lX = 0;
    mouseState_.lY = 0;
}

bool Input::IsGamepadConnected() const
{
    return isGamepadConnected_;
}

bool Input::IsGamepadButtonPressed(WORD button) const
{
    return isGamepadConnected_ &&
        (gamepadState_.Gamepad.wButtons & button) != 0;
}

bool Input::IsGamepadButtonTrigger(WORD button) const
{
    const bool isPressed = (gamepadState_.Gamepad.wButtons & button) != 0;
    const bool wasPressed = (preGamepadState_.Gamepad.wButtons & button) != 0;
    return isGamepadConnected_ && isPressed && !wasPressed;
}

namespace {
float NormalizeStickAxis(SHORT value, SHORT otherAxis, SHORT deadZone)
{
    const float x = static_cast<float>(value);
    const float y = static_cast<float>(otherAxis);
    const float magnitude = std::sqrt(x * x + y * y);

    if (magnitude <= static_cast<float>(deadZone)) {
        return 0.0f;
    }

    const float clampedMagnitude = (std::min)(magnitude, 32767.0f);
    const float normalizedMagnitude =
        (clampedMagnitude - static_cast<float>(deadZone)) /
        (32767.0f - static_cast<float>(deadZone));
    return (x / magnitude) * normalizedMagnitude;
}
}

float Input::GetGamepadLeftStickX() const
{
    if (!isGamepadConnected_) {
        return 0.0f;
    }

    return NormalizeStickAxis(
        gamepadState_.Gamepad.sThumbLX,
        gamepadState_.Gamepad.sThumbLY,
        XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
}

float Input::GetGamepadLeftStickY() const
{
    if (!isGamepadConnected_) {
        return 0.0f;
    }

    return NormalizeStickAxis(
        gamepadState_.Gamepad.sThumbLY,
        gamepadState_.Gamepad.sThumbLX,
        XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
}

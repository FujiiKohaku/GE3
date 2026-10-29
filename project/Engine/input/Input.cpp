#include "Input.h"
#include "Engine/Logger/Logger.h"
#include <cassert>
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "xinput.lib")

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <sstream>
std::unique_ptr<Input> Input::instance_ = nullptr;


Input* Input::GetInstance()
{
    if (!instance_) {
        instance_ = std::make_unique<Input>(ConstructorKey());
    }
    return instance_.get();
}

void Input::SetGamepadAimSensitivity(float sensitivity)
{
    if (std::isfinite(sensitivity)) {
        gamepadAimSensitivity_ = std::clamp(sensitivity, 0.5f, 2.0f);
    }
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
    const DWORD previousGamepadIndex = gamepadIndex_;
    activeGamepadDisconnected_ = false;
    std::memset(keys_, 0, sizeof(keys_));
    mouseState_ = {};

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
    std::array<DWORD, XUSER_MAX_COUNT> gamepadPollResults;
    gamepadPollResults.fill(ERROR_NOT_READY);

    for (DWORD index = 0; index < XUSER_MAX_COUNT; ++index) {
        gamepadPollResults[index] = XInputGetState(index, &nextGamepadState);
        if (gamepadPollResults[index] == ERROR_SUCCESS) {
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
            preGamepadState_ = nextGamepadState;
            gamepadButtonsReady_ = false;
        }
        gamepadState_ = nextGamepadState;
        gamepadIndex_ = nextGamepadIndex;
    }

    activeGamepadDisconnected_ = isUsingGamepad_ &&
        previousGamepadIndex < XUSER_MAX_COUNT && previousGamepadIndex != gamepadIndex_;
    if (!isGamepadConnected_) {
        gamepadButtonsReady_ = false;
        isUsingGamepad_ = false;
    } else if (gamepadState_.Gamepad.wButtons == 0 &&
        gamepadState_.Gamepad.bRightTrigger <= XINPUT_GAMEPAD_TRIGGER_THRESHOLD &&
        gamepadState_.Gamepad.bLeftTrigger <= XINPUT_GAMEPAD_TRIGGER_THRESHOLD) {
        // 接続直後に押されていたボタンは、一度離すまで受け付けない。
        gamepadButtonsReady_ = true;
    }

    const bool isForeground = GetForegroundWindow() == winApp_->GetHwnd();
    if (!isForeground) {
        std::memset(keys_, 0, sizeof(keys_));
        mouseState_ = {};
        gamepadState_ = {};
        gamepadButtonsReady_ = false;
        blockPrimaryFire_ = true;
        LogGamepadDiagnostics(nextGamepadState, gamepadPollResults.data(), isForeground);
        return;
    }

    // IMEなどのキー状態でパッド操作を解除しないよう、ゲームの操作キーだけを調べる。
    constexpr BYTE kControlKeys[] = {
        DIK_W, DIK_A, DIK_S, DIK_D,
        DIK_SPACE, DIK_LSHIFT, DIK_LCONTROL, DIK_RCONTROL,
        DIK_1, DIK_2, DIK_3, DIK_4,
        DIK_UP, DIK_DOWN, DIK_LEFT, DIK_RIGHT,
        DIK_TAB, DIK_RETURN, DIK_ESCAPE, DIK_BACK,
        DIK_LBRACKET, DIK_RBRACKET, DIK_R, DIK_T, DIK_C
    };
    bool hasKeyboardActivity = false;
    for (const BYTE keyCode : kControlKeys) {
        if (IsKeyPressed(keyCode)) {
            hasKeyboardActivity = true;
            break;
        }
    }
    const bool gamepadActivity = gamepadButtonsReady_ &&
        (gamepadState_.Gamepad.wButtons != 0 || IsGamepadRightTriggerPressed() ||
         GetGamepadLeftStickX() != 0.0f || GetGamepadLeftStickY() != 0.0f);
    const bool mouseActivity = mouseState_.lX != 0 || mouseState_.lY != 0 ||
        mouseState_.lZ != 0 || IsMousePressed(0) || IsMousePressed(1);
    if (hasKeyboardActivity) {
        isUsingGamepad_ = false;
    } else if (gamepadActivity) {
        isUsingGamepad_ = true;
    } else if (mouseActivity) {
        isUsingGamepad_ = false;
    }
    if (blockPrimaryFire_ && !IsKeyPressed(DIK_SPACE) && !IsMousePressed(0) &&
        !IsGamepadButtonPressed(XINPUT_GAMEPAD_A) && !IsGamepadRightTriggerPressed()) {
        blockPrimaryFire_ = false;
    }
    LogGamepadDiagnostics(nextGamepadState, gamepadPollResults.data(), isForeground);
}

// 入力を変更せず、取得値と操作方式の判定を約1秒ごとに記録する。
void Input::LogGamepadDiagnostics(const XINPUT_STATE& polledState, const DWORD* pollResults, bool isForeground)
{
    constexpr ULONGLONG kLogIntervalMs = 1000;
    if (isGamepadConnected_) {
        gamepadDiagnosticsPeakLeftX_ = (std::max)(gamepadDiagnosticsPeakLeftX_,
            std::abs(static_cast<int>(polledState.Gamepad.sThumbLX)));
        gamepadDiagnosticsPeakLeftY_ = (std::max)(gamepadDiagnosticsPeakLeftY_,
            std::abs(static_cast<int>(polledState.Gamepad.sThumbLY)));
    } else {
        ++gamepadDiagnosticsDisconnectedFrames_;
    }
    const ULONGLONG currentTimeMs = GetTickCount64();
    if (lastGamepadDiagnosticsTimeMs_ != 0 && currentTimeMs - lastGamepadDiagnosticsTimeMs_ < kLogIntervalMs) {
        return;
    }
    lastGamepadDiagnosticsTimeMs_ = currentTimeMs;

    std::ostringstream message;
    message << std::boolalpha << "[GamepadDiagnostics] connected=" << isGamepadConnected_
        << " slot=" << gamepadIndex_ << " foreground=" << isForeground
        << " usingGamepad=" << isUsingGamepad_ << " buttonsReady=" << gamepadButtonsReady_
        << " disconnectedFrames=" << gamepadDiagnosticsDisconnectedFrames_ << " pollResults=[";
    for (DWORD index = 0; index < XUSER_MAX_COUNT; ++index) {
        if (index != 0) { message << ','; }
        if (index > gamepadIndex_) {
            message << "notPolled";
        } else {
            message << pollResults[index];
        }
    }
    message << "] packet=" << polledState.dwPacketNumber
        << " rawLeft=(" << polledState.Gamepad.sThumbLX << ',' << polledState.Gamepad.sThumbLY << ')'
        << " peakAbsLeft=(" << gamepadDiagnosticsPeakLeftX_ << ',' << gamepadDiagnosticsPeakLeftY_ << ')'
        << " normalizedLeft=(" << GetGamepadLeftStickX() << ',' << GetGamepadLeftStickY() << ')'
        << " rawRight=(" << polledState.Gamepad.sThumbRX << ',' << polledState.Gamepad.sThumbRY << ')'
        << " buttons=" << polledState.Gamepad.wButtons
        << " triggers=(" << static_cast<int>(polledState.Gamepad.bLeftTrigger)
        << ',' << static_cast<int>(polledState.Gamepad.bRightTrigger) << ") keyboardScanCodes=[";
    bool hasPressedKey = false;
    for (int keyIndex = 0; keyIndex < 256; ++keyIndex) {
        if ((keys_[keyIndex] & 0x80) == 0) { continue; }
        if (hasPressedKey) { message << ','; }
        message << keyIndex;
        hasPressedKey = true;
    }
    message << "] mouseDelta=(" << mouseState_.lX << ',' << mouseState_.lY << ')';
    Logger::Log(message.str());
    Logger::Flush();
    gamepadDiagnosticsPeakLeftX_ = 0;
    gamepadDiagnosticsPeakLeftY_ = 0;
    gamepadDiagnosticsDisconnectedFrames_ = 0;
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
    return isGamepadConnected_ && gamepadButtonsReady_ &&
        (gamepadState_.Gamepad.wButtons & button) != 0;
}

bool Input::IsGamepadButtonTrigger(WORD button) const
{
    const bool isPressed = (gamepadState_.Gamepad.wButtons & button) != 0;
    const bool wasPressed = (preGamepadState_.Gamepad.wButtons & button) != 0;
    return isGamepadConnected_ && gamepadButtonsReady_ && isPressed && !wasPressed;
}

bool Input::IsGamepadRightTriggerPressed() const
{
    return isGamepadConnected_ && gamepadButtonsReady_ &&
        gamepadState_.Gamepad.bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
}

bool Input::IsPrimaryFirePressed() const
{
    return !blockPrimaryFire_ && (IsKeyPressed(DIK_SPACE) || IsMousePressed(0) ||
        IsGamepadButtonPressed(XINPUT_GAMEPAD_A) || IsGamepadRightTriggerPressed());
}

bool Input::IsPrimaryFireTrigger() const
{
    const bool wasPressed = (preKeys_[DIK_SPACE] & 0x80) != 0 ||
        (preMouseState_.rgbButtons[0] & 0x80) != 0 ||
        (preGamepadState_.Gamepad.wButtons & XINPUT_GAMEPAD_A) != 0 ||
        preGamepadState_.Gamepad.bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
    return IsPrimaryFirePressed() && !wasPressed;
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

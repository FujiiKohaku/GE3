#pragma once
#include "Engine/Winapp/WinApp.h"
#include <dinput.h>
#include <xinput.h>
#include <memory>
#include <wrl.h>

class Input {
public:
    static Input* GetInstance();

    bool Initialize(WinApp* winApp);
    void Update();
    void Finalize();

    bool IsKeyPressed(BYTE keyCode) const; // 押されているか

    bool IsMousePressed(int button) const; // 押されているか

    bool IsKeyTrigger(BYTE keyCode) const; // 一回だけ押されたか
    bool IsMouseTrigger(int button) const; // 一回だけ押されたか
    LONG GetMouseDeltaX() const;
    LONG GetMouseDeltaY() const;
    LONG GetMouseWheel() const;
    void ResetMouseDelta();

    bool IsGamepadConnected() const;
    bool IsGamepadButtonPressed(WORD button) const;
    bool IsGamepadButtonTrigger(WORD button) const;
    float GetGamepadLeftStickX() const;
    float GetGamepadLeftStickY() const;
    bool IsGamepadRightTriggerPressed() const;
    bool IsUsingGamepad() const { return isUsingGamepad_; }
    bool WasActiveGamepadDisconnected() const { return activeGamepadDisconnected_; }
    bool IsPrimaryFirePressed() const;
    bool IsPrimaryFireTrigger() const;
    void BlockPrimaryFireUntilRelease() { blockPrimaryFire_ = true; }
    bool IsPrimaryFireBlocked() const { return blockPrimaryFire_; }
    bool IsGamepadPitchInverted() const { return gamepadPitchInverted_; }
    void ToggleGamepadPitchInverted() { gamepadPitchInverted_ = !gamepadPitchInverted_; }
    float GetGamepadAimSensitivity() const { return gamepadAimSensitivity_; }
    void SetGamepadAimSensitivity(float sensitivity);

public:
    class ConstructorKey {
    private:
        ConstructorKey() = default;
        friend class Input;
    };

    Input(ConstructorKey) { }
    ~Input() = default;

private:
    friend struct InputTestAccess;
    Input(const Input&) = delete;
    Input& operator=(const Input&) = delete;
    void LogGamepadDiagnostics(const XINPUT_STATE& polledState, const DWORD* pollResults, bool isForeground);

private:
    static std::unique_ptr<Input> instance_;

    WinApp* winApp_ = nullptr;

    Microsoft::WRL::ComPtr<IDirectInput8> directInput_;
    Microsoft::WRL::ComPtr<IDirectInputDevice8> keyboard_;
    Microsoft::WRL::ComPtr<IDirectInputDevice8> mouse_;


    BYTE preKeys_[256] = {};
    DIMOUSESTATE2 preMouseState_ = {};

    BYTE keys_[256] = {};
    DIMOUSESTATE2 mouseState_ = {};
    XINPUT_STATE preGamepadState_ = {};
    XINPUT_STATE gamepadState_ = {};
    bool isGamepadConnected_ = false;
    DWORD gamepadIndex_ = XUSER_MAX_COUNT;
    bool isUsingGamepad_ = false;
    bool activeGamepadDisconnected_ = false;
    bool gamepadButtonsReady_ = false;
    bool blockPrimaryFire_ = false;
    bool gamepadPitchInverted_ = false;
    float gamepadAimSensitivity_ = 1.0f;
    ULONGLONG lastGamepadDiagnosticsTimeMs_ = 0;
    int gamepadDiagnosticsPeakLeftX_ = 0;
    int gamepadDiagnosticsPeakLeftY_ = 0;
    unsigned int gamepadDiagnosticsDisconnectedFrames_ = 0;
};

#include "Input.h"
#include <cassert>
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")
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
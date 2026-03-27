#include "Input.h"
#include <cassert>

std::unique_ptr<Input> Input::instance_ = nullptr;

Input::Input(ConstructorKey)
{
}

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

    return true;
}

void Input::Update()
{
    HRESULT result = keyboard_->Acquire();
    if (FAILED(result)) {
        return;
    }

    result = keyboard_->GetDeviceState(sizeof(keys_), keys_);
    if (FAILED(result)) {
        keyboard_->Acquire();
    }
}

bool Input::IsKeyPressed(BYTE keyCode) const
{
    return (keys_[keyCode] & 0x80) != 0;
}

void Input::Finalize()
{
    if (!instance_) {
        return;
    }

    instance_->keyboard_.Reset();
    instance_->directInput_.Reset();
    instance_.reset();
}
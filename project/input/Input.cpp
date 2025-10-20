#include "Input.h"
#include <cassert>

bool Input::Initialize(WinApp* winApp)
{
    HRESULT result;

    winApp_ = winApp;
    // DirectInput全体の初期化(後からゲームパッドなどを追加するとしてもこのオブジェクトはひとつでいい)(winmainを改造、hinstanceに名づけをしました)
    result = DirectInput8Create(winApp_->GetHinstance(), DIRECTINPUT_VERSION, IID_IDirectInput8,
        reinterpret_cast<void**>(directInput_.ReleaseAndGetAddressOf()), nullptr);
    assert(SUCCEEDED(result));

    // キーボードデバイスの生成（GUID_Joystickなど指定すればほかの種類のデバイスも扱える）
    result = directInput_->CreateDevice(GUID_SysKeyboard, keyboard_.ReleaseAndGetAddressOf(), nullptr);
    assert(SUCCEEDED(result));

    // 入六データ形式のセット(キーボードの場合c_dfDIKeyboardだけど入力デバイスの種類によってあらかじめ何種類か用意されている)
    result = keyboard_->SetDataFormat(&c_dfDIKeyboard);
    assert(SUCCEEDED(result));

    // 排他制御レベルのセット
    result = keyboard_->SetCooperativeLevel(winApp_->GetHwnd(), DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
    assert(SUCCEEDED(result));

    return true;
}

void Input::Update()
{
    HRESULT result = keyboard_->Acquire();
    if (FAILED(result)) {
        // 取得失敗（ウィンドウ非アクティブなど）なら何もしない
        return;
    }

    result = keyboard_->GetDeviceState(sizeof(keys_), keys_);
    if (FAILED(result)) {
        // 失敗時は再取得を試みる（オプション）
        keyboard_->Acquire();
    }
}
bool Input::IsKeyPressed(BYTE keyCode) const
{
    return keys_[keyCode] & 0x80;
}
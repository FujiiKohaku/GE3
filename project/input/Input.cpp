#include "Input.h"
#include <cassert>

Input* Input::instance = nullptr;
// ================================
// Singletonインスタンス取得
// ================================
Input* Input::GetInstance()
{
    if (!instance) {
        instance = new Input();
    }
    return instance;
}

// 初期化
bool Input::Initialize(WinApp* winApp)
{
    HRESULT result;

    winApp_ = winApp;
    // DirectInput全体の初期化(後からゲームパッドなどを追加するとしてもこのオブジェクトはひとつでいい)(winmainを改造、hinstanceに名づけをしました)
    result = DirectInput8Create(winApp_->GetHinstance(), DIRECTINPUT_VERSION, IID_IDirectInput8,
        reinterpret_cast<void**>(directInput_.ReleaseAndGetAddressOf()), nullptr);
    assert(SUCCEEDED(result));

    // キーボードデバイスの生成（GUID_Joystickなど指定すればほかの種類のデバイスも扱える）::キーボード使わせてくださいとお願いしている
    result = directInput_->CreateDevice(GUID_SysKeyboard, keyboard_.ReleaseAndGetAddressOf(), nullptr); // 「DirectInput にキーボードデバイスを作らせて、その結果（成功/失敗）を result に入れて、成功なら keyboard_ にキーボードの実体が入る」というコード
    assert(SUCCEEDED(result));

    // 入六データ形式のセット(キーボードの場合c_dfDIKeyboardだけど入力デバイスの種類によってあらかじめ何種類か用意されている)
    result = keyboard_->SetDataFormat(&c_dfDIKeyboard);
    assert(SUCCEEDED(result));

    // 排他制御レベルのセット
    result = keyboard_->SetCooperativeLevel(winApp_->GetHwnd(), DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
    assert(SUCCEEDED(result));
    // DISCL_FOREGROUND：ウィンドウが前面にあるときだけ入力を受け取る
    // DISCL_NONEXCLUSIVE：他のアプリと入力を共有する
    //  DISCL_NOWINKEY Windowsキーを無効にする

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
void Input::Finalize()
{
    // 明示的に解放
    keyboard_.Reset();
    directInput_.Reset();

    delete instance;
    instance = nullptr;
}
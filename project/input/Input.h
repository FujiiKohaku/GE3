#pragma once
#define DIRECTINPUT_VERSION 0x0800 // DirectInputのバージョン指定
#include "WinApp.h"
#include <dinput.h>
#include <wrl.h> // ComPtr 用
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

class Input {
public:
    bool Initialize(WinApp* winApp);
    void Update();
    bool IsKeyPressed(BYTE keyCode) const;

private:
    Microsoft::WRL::ComPtr<IDirectInput8> directInput_ = nullptr;
    Microsoft::WRL::ComPtr<IDirectInputDevice8> keyboard_;
    BYTE keys_[256] {};
    // WinDowsAPI
    WinApp* winApp_ = nullptr;
};

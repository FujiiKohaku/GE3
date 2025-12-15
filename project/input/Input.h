#pragma once
#define DIRECTINPUT_VERSION 0x0800 // DirectInputのバージョン指定

#include "WinApp.h"
#include <dinput.h>
#include <wrl.h> // ComPtr 用

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

class Input {
public:
    // ================================
    // Singletonインスタンス取得
    // ================================
    static Input* GetInstance();

    // ================================
    // 初期化
    // ================================
    bool Initialize(WinApp* winApp);
    // ================================
    // 終了
    // ================================
    void Finalize();
    // ================================
    // 毎フレーム入力更新
    // ================================
    void Update();

    // ================================
    // キー押下判定
    // ================================
    bool IsKeyPressed(BYTE keyCode) const;

private:
    // コンストラクタ／デストラクタは非公開
    Input() = default;
    ~Input() = default;

    // コピー禁止
    Input(const Input&) = delete;
    Input& operator=(const Input&) = delete;

private:
    Microsoft::WRL::ComPtr<IDirectInput8> directInput_ = nullptr;
    Microsoft::WRL::ComPtr<IDirectInputDevice8> keyboard_;
    BYTE keys_[256] {};

    WinApp* winApp_ = nullptr;

    static Input* instance;
};

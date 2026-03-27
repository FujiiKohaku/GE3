#pragma once
#define DIRECTINPUT_VERSION 0x0800

#include "WinApp.h"
#include <dinput.h>
#include <memory>
#include <wrl.h>

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

class Input {
public:
    static Input* GetInstance();
    static void Finalize();

    bool Initialize(WinApp* winApp);
    void Update();
    bool IsKeyPressed(BYTE keyCode) const;

private:
    static std::unique_ptr<Input> instance_;

    Input(const Input&) = delete;
    Input& operator=(const Input&) = delete;

public:
    class ConstructorKey {
        ConstructorKey() = default;
        friend class Input;
    };

    explicit Input(ConstructorKey);
    ~Input() = default;

private:
    Microsoft::WRL::ComPtr<IDirectInput8> directInput_ = nullptr;
    Microsoft::WRL::ComPtr<IDirectInputDevice8> keyboard_;
    BYTE keys_[256] {};

    WinApp* winApp_ = nullptr;
};
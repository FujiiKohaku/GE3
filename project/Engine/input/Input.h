#pragma once
#include "Engine/Winapp/WinApp.h"
#include <dinput.h>
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

public:
    class ConstructorKey {
    private:
        ConstructorKey() = default;
        friend class Input;
    };

    Input(ConstructorKey) { }
    ~Input() = default;

private:
    Input(const Input&) = delete;
    Input& operator=(const Input&) = delete;

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


};
#include "WinApp.h"
#include "imgui/imgui.h" // ImGui 本体
#include "imgui/imgui_impl_dx12.h" // DirectX12 連携
#include "imgui/imgui_impl_win32.h" // Win32 連携
#include <Windows.h>
#include <cstdint>


std::unique_ptr<WinApp> WinApp::instance_ = nullptr;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace {
constexpr DWORD kWindowStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
}


WinApp* WinApp::GetInstance()
{
    if (!instance_) {
        instance_ = std::make_unique<WinApp>(ConstructorKey());
    }

    return instance_.get();
}

void WinApp::FinalizeInstance()
{
    if (!instance_) {
        return;
    }

    instance_->Finalize();
    instance_.reset();
}






//==================================================================
//  ウィンドウプロシージャ
//  Windowsからのメッセージを処理する
//==================================================================
LRESULT CALLBACK WinApp::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    // ImGui用メッセージ処理（優先）
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
        return true;
    }

    // メッセージに応じた固有処理
    switch (msg) {
    case WM_DESTROY: // ウィンドウが破棄された
        PostQuitMessage(0); // OSにアプリ終了を通知
        return 0;
    }

    // 標準のメッセージ処理を実行
    return DefWindowProc(hwnd, msg, wparam, lparam);
}

//==================================================================
//  メッセージ処理関数
//  戻り値 true: 終了メッセージを受け取った
//==================================================================
bool WinApp::ProcessMessage()
{
    MSG msg {};
    if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // 終了メッセージを検出
    if (msg.message == WM_QUIT) {
        return true;
    }

    return false;
}

int32_t WinApp::GetClientWidth() const
{
    if (hwnd_ == nullptr) {
        return kClientWidth;
    }

    RECT clientRect {};
    GetClientRect(hwnd_, &clientRect);
    return clientRect.right - clientRect.left;
}

int32_t WinApp::GetClientHeight() const
{
    if (hwnd_ == nullptr) {
        return kClientHeight;
    }

    RECT clientRect {};
    GetClientRect(hwnd_, &clientRect);
    return clientRect.bottom - clientRect.top;
}

//==================================================================
//  初期化処理
//  ウィンドウクラス登録・生成・表示
//==================================================================
void WinApp::initialize()
{
    // DPI
    SetProcessDPIAware();

    // COMライブラリ初期化（マルチスレッド対応）
    CoInitializeEx(0, COINIT_MULTITHREADED);

    // ウィンドウクラス設定
    wc_.lpfnWndProc = WindowProc; // ウィンドウプロシージャ
    wc_.lpszClassName = L"CG2WindowClass"; // クラス名
    wc_.hInstance = GetModuleHandle(nullptr); // インスタンスハンドル
    wc_.hCursor = LoadCursor(nullptr, IDC_ARROW); // カーソル設定

    // ウィンドウクラスを登録
    RegisterClass(&wc_);

    // クライアント領域を元にウィンドウサイズを調整
    RECT wrc = { 0, 0, kClientWidth, kClientHeight };
    AdjustWindowRect(&wrc, kWindowStyle, false);

    // ウィンドウ生成
    hwnd_ = CreateWindow(
        wc_.lpszClassName, // クラス名
        L"LE3B_21_フジイ_コハク", // タイトル
        kWindowStyle, // スタイル
        CW_USEDEFAULT, CW_USEDEFAULT, // 位置（自動）
        wrc.right - wrc.left, // 幅
        wrc.bottom - wrc.top, // 高さ
        nullptr, nullptr, wc_.hInstance, nullptr);

    // ウィンドウ表示
    ShowWindow(hwnd_, SW_SHOW);
}

//==================================================================
//  終了処理
//  ウィンドウ破棄とCOM解放
//==================================================================
void WinApp::Finalize()
{
    CloseWindow(hwnd_);
    CoUninitialize();
}

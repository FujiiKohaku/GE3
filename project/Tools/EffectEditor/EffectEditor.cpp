#include "EffectEditorApp.h"
#include "Engine/Logger/Logger.h"
#include <fstream>

namespace {
EffectEditorApp* activeEditor = nullptr;
WNDPROC previousWindowProc = nullptr;
LRESULT CALLBACK EditorWindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (message == WM_CLOSE && activeEditor != nullptr) { activeEditor->RequestClose(); return 0; }
    return CallWindowProcW(previousWindowProc, window, message, wparam, lparam);
}
void FindResources()
{
    wchar_t executable[32768] {};
    GetModuleFileNameW(nullptr, executable, _countof(executable));
    auto directory = std::filesystem::path(executable).parent_path();
    for (int depth = 0; depth < 7; ++depth) {
        if (std::filesystem::is_directory(directory / "project/resources/Effects")) {
            std::filesystem::current_path(directory / "project"); return;
        }
        directory = directory.parent_path();
    }
    if (!std::filesystem::is_directory("resources/Effects")) { throw std::runtime_error("resources/Effectsが見つかりません"); }
}
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR commandLine, int)
{
    int exitCode = 0;
    const bool isSmokeTest = std::string(commandLine).find("--smoke-test") != std::string::npos;
    EffectEditorApp app;
    try {
        FindResources(); Logger::Initialize();
        app.Initialize();
        if (isSmokeTest) { app.RunSmokeTest(); }
        else {
            activeEditor = &app;
            previousWindowProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(WinApp::GetInstance()->GetHwnd(), GWLP_WNDPROC,
                reinterpret_cast<LONG_PTR>(EditorWindowProc)));
            WinApp::GetInstance()->Show();
            while (!app.ShouldQuit() && !WinApp::GetInstance()->ProcessMessage()) { app.Update(); app.Draw(); }
            SetWindowLongPtrW(WinApp::GetInstance()->GetHwnd(), GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(previousWindowProc));
            activeEditor = nullptr;
        }
        app.Finalize();
    } catch (const std::exception& error) {
        Logger::Error(error.what()); Logger::Flush();
        activeEditor = nullptr;
        if (previousWindowProc != nullptr) {
            SetWindowLongPtrW(WinApp::GetInstance()->GetHwnd(), GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(previousWindowProc));
        }
        try { app.Finalize(); } catch (const std::exception& finalizeError) { Logger::Error(finalizeError.what()); }
        std::filesystem::create_directories("runtime/EffectEditorSmoke");
        std::ofstream("runtime/EffectEditorSmoke/failure.txt") << error.what();
        OutputDebugStringA(error.what()); exitCode = 1;
        if (!isSmokeTest) { MessageBoxW(nullptr, L"エディターを起動または実行できませんでした。runtime/EffectEditorSmoke/failure.txt とログを確認してください。", L"Kohaku Effect Editor", MB_OK | MB_ICONERROR); }
    }
    activeEditor = nullptr;
    Logger::Finalize(); WinApp::FinalizeInstance();
    return exitCode;
}

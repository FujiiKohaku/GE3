#include "StandaloneD3D12.h"
#include "UIAnimationEditorApp.h"
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"
#include <Windows.h>
#include <chrono>
#include <filesystem>
#include <shellapi.h>
#include <string>

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

static UIAnimationEditorApp* g_editorApp = nullptr;
static UIEditorD3D12* g_d3d12 = nullptr;
static bool g_isWindowMinimized = false;

namespace {
constexpr float kImGuiFontSize = 19.0f;
const char* kNotoSansJpFontPaths[] = {
    "Noto_Sans_JP/static/NotoSansJP-Regular.ttf",
    "project/Noto_Sans_JP/static/NotoSansJP-Regular.ttf",
    "../Noto_Sans_JP/static/NotoSansJP-Regular.ttf",
    "../../Noto_Sans_JP/static/NotoSansJP-Regular.ttf",
    "../../../Noto_Sans_JP/static/NotoSansJP-Regular.ttf",
    "../../../../Noto_Sans_JP/static/NotoSansJP-Regular.ttf",
    "../../../project/Noto_Sans_JP/static/NotoSansJP-Regular.ttf",
};

void LogEditorMessage(const std::string& message)
{
    std::string line = message + "\n";
    OutputDebugStringA(line.c_str());
}

void LogNotoSansJpMissing()
{
    std::string message = "Noto Sans JP font was not found. Searched paths:";

    for (const char* fontPath : kNotoSansJpFontPaths) {
        message += " ";
        message += fontPath;
    }

    LogEditorMessage(message);
}

void RegisterNotoSansJpFont(ImGuiIO& io)
{
    for (const char* fontPath : kNotoSansJpFontPaths) {
        if (!std::filesystem::exists(fontPath)) {
            continue;
        }

        ImFont* font = io.Fonts->AddFontFromFileTTF(
            fontPath,
            kImGuiFontSize,
            nullptr,
            io.Fonts->GetGlyphRangesJapanese());

        if (font != nullptr) {
            io.FontDefault = font;
            LogEditorMessage(std::string("Loaded ImGui font: ") + fontPath);
            return;
        }

        LogEditorMessage(std::string("Failed to load ImGui font file: ") + fontPath);
        return;
    }

    LogNotoSansJpMissing();
}
}

LRESULT CALLBACK UIAnimationEditorWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hwnd, message, wParam, lParam)) {
        return true;
    }

    if (message == WM_DROPFILES) {
        HDROP dropHandle = reinterpret_cast<HDROP>(wParam);
        wchar_t filePath[MAX_PATH] = {};

        if (DragQueryFileW(dropHandle, 0, filePath, MAX_PATH) > 0) {
            if (g_editorApp != nullptr) {
                g_editorApp->OnDropFile(filePath);
            }
        }

        DragFinish(dropHandle);
        return 0;
    }

    if (message == WM_SIZE) {
        if (wParam == SIZE_MINIMIZED) {
            g_isWindowMinimized = true;
            return 0;
        }

        g_isWindowMinimized = false;

        if (g_d3d12 != nullptr) {
            int width = static_cast<int>(LOWORD(lParam));
            int height = static_cast<int>(HIWORD(lParam));
            g_d3d12->Resize(width, height);
        }

        return 0;
    }

    if (message == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    const wchar_t* className = L"UIAnimationEditorWindow";

    WNDCLASSEXW windowClass = {};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_CLASSDC;
    windowClass.lpfnWndProc = UIAnimationEditorWndProc;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = className;
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassExW(&windowClass);

    RECT windowRect = { 0, 0, 1600, 900 };
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hwnd = CreateWindowW(
        className,
        L"UIAnimationEditor",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        windowRect.right - windowRect.left,
        windowRect.bottom - windowRect.top,
        nullptr,
        nullptr,
        instance,
        nullptr);

    if (hwnd == nullptr) {
        CoUninitialize();
        return 1;
    }

    UIEditorD3D12 d3d12;
    RECT clientRect = {};
    GetClientRect(hwnd, &clientRect);
    int clientWidth = clientRect.right - clientRect.left;
    int clientHeight = clientRect.bottom - clientRect.top;

    if (!d3d12.Initialize(hwnd, clientWidth, clientHeight)) {
        CoUninitialize();
        return 1;
    }

    g_d3d12 = &d3d12;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    RegisterNotoSansJpFont(io);

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX12_Init(
        d3d12.GetDevice(),
        2,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        d3d12.GetSrvDescriptorHeap(),
        d3d12.GetFontCpuHandle(),
        d3d12.GetFontGpuHandle());

    UIAnimationEditorApp editorApp;
    editorApp.Initialize(hwnd, &d3d12);
    g_editorApp = &editorApp;

    DragAcceptFiles(hwnd, TRUE);
    ShowWindow(hwnd, showCommand);
    UpdateWindow(hwnd);

    bool isRunning = true;
    MSG message = {};
    std::chrono::steady_clock::time_point previousTime = std::chrono::steady_clock::now();

    while (isRunning) {
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);

            if (message.message == WM_QUIT) {
                isRunning = false;
            }
        }

        if (!isRunning) {
            break;
        }

        if (g_isWindowMinimized) {
            previousTime = std::chrono::steady_clock::now();
            Sleep(16);
            continue;
        }

        std::chrono::steady_clock::time_point currentTime = std::chrono::steady_clock::now();
        std::chrono::duration<float> elapsedTime = currentTime - previousTime;
        previousTime = currentTime;

        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        editorApp.Update(elapsedTime.count());
        editorApp.Draw();

        ImGui::Render();
        d3d12.BeginFrame();
        d3d12.EndFrame();
    }

    d3d12.WaitForGpu();
    g_editorApp = nullptr;
    g_d3d12 = nullptr;
    DragAcceptFiles(hwnd, FALSE);

    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    d3d12.Shutdown();
    DestroyWindow(hwnd);
    UnregisterClassW(className, instance);
    CoUninitialize();

    return 0;
}

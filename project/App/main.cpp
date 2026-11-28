#include "Engine/Debug/Profiler/StartupTiming.h"
// アプリの入口。終了要求を受けるまでゲームの更新と描画を繰り返す。
#include "Engine/ImGuiManager/ImGuiManager.h"
#include "Engine/Logger/Logger.h"
#include "App/Scene/Application/Game.h"
#include "App/Scene/Gameplay/GamePlayScene.h"
#include "App/Scene/Loading/LoadingScene.h"
#include "Engine/Screenshot/ScreenshotManager.h"

#include <chrono>
#ifdef _DEBUG
#include "Tests/CameraInitializationTests.h"
#include "Tests/PlayerWeaponPolymorphismTests.h"
#include "Tests/TitleStateTests.h"
#include "Tests/BeamStateTests.h"
#include "Tests/JellyfishAttackTests.h"
#include "Tests/EnemyDebugViewTests.h"
#include "Tests/BossSpawnTests.h"
#include "Tests/BossPresentationTests.h"
#include "Tests/EnemyAppearanceTests.h"
#include "Tests/EnemyMovementTests.h"
#include "Tests/SwarmFormationTests.h"
#include "Tests/FearWormMovementTests.h"
#include "Tests/FearWormEntryTests.h"
#include "Tests/FearWormStateTests.h"
#include "Tests/PlayerControlTests.h"
#include <cstring>
#endif

namespace {
constexpr wchar_t kStartupSplashClass[] = L"KohakuEngineStartupSplash";

LRESULT CALLBACK StartupSplashWindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (message != WM_PAINT) {
        return DefWindowProcW(window, message, wparam, lparam);
    }

    PAINTSTRUCT paint{};
    HDC dc = BeginPaint(window, &paint);
    RECT client{};
    GetClientRect(window, &client);
    SetDCBrushColor(dc, RGB(18, 16, 14));
    FillRect(dc, &client, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
    RECT accent{client.left, client.bottom - 3, client.right, client.bottom};
    SetDCBrushColor(dc, RGB(201, 120, 42));
    FillRect(dc, &accent, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(249, 183, 99));
    HFONT titleFont = CreateFontW(-38, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH, L"Segoe UI");
    HGDIOBJ previousFont = SelectObject(dc, titleFont ? titleFont : GetStockObject(DEFAULT_GUI_FONT));
    RECT title{client.left, 0, client.right, client.bottom - 24};
    DrawTextW(dc, L"KohakuEngine", -1, &title, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT));
    SetTextColor(dc, RGB(209, 197, 184));
    RECT status{client.left, client.bottom / 2 + 24, client.right, client.bottom - 3};
    DrawTextW(dc, L"\u8d77\u52d5\u4e2d\u2026", -1, &status, DT_CENTER | DT_SINGLELINE);
    SelectObject(dc, previousFont);
    if (titleFont) {
        DeleteObject(titleFont);
    }
    EndPaint(window, &paint);
    return 0;
}

class StartupSplash {
public:
    explicit StartupSplash(HINSTANCE instance) : instance_(instance)
    {
        WNDCLASSW windowClass{};
        windowClass.lpfnWndProc = StartupSplashWindowProc;
        windowClass.hInstance = instance_;
        windowClass.lpszClassName = kStartupSplashClass;
        if (!RegisterClassW(&windowClass)) {
            return;
        }
        registered_ = true;

        POINT cursor{};
        GetCursorPos(&cursor);
        MONITORINFO monitor{};
        monitor.cbSize = sizeof(monitor);
        GetMonitorInfoW(MonitorFromPoint(cursor, MONITOR_DEFAULTTOPRIMARY), &monitor);
        const RECT& work = monitor.rcWork;
        constexpr int width = 480;
        constexpr int height = 240;
        window_ = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
            kStartupSplashClass, L"KohakuEngine", WS_POPUP,
            work.left + (work.right - work.left - width) / 2,
            work.top + (work.bottom - work.top - height) / 2, width, height,
            nullptr, nullptr, instance_, nullptr);
        if (window_) {
            ShowWindow(window_, SW_SHOWNOACTIVATE);
            UpdateWindow(window_);
        }
    }

    ~StartupSplash()
    {
        Close();
        if (registered_) {
            UnregisterClassW(kStartupSplashClass, instance_);
        }
    }

    void Close()
    {
        if (window_) {
            DestroyWindow(window_);
            window_ = nullptr;
        }
    }

private:
    HINSTANCE instance_;
    HWND window_ = nullptr;
    bool registered_ = false;
};
}

// 起動時の準備を行い、入力・更新・描画を繰り返した後にゲームを終了する。
int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR commandLine, int)
{
// デバッグ用の検証指定がある場合は、通常のゲーム起動より先に実行する。
#ifdef _DEBUG
    if (std::strstr(commandLine, "--verify-camera-initialization")) {
        return RunCameraInitializationTests() ? 0 : 1;
    }
    if (std::strstr(commandLine, "--verify-fear-worm-states")) {
        if (RunFearWormStateTests()) { return 0; }
        return 1;
    }
    if (std::strstr(commandLine, "--verify-fear-worm-entry")) {
        if (RunFearWormEntryTests()) { return 0; }
        return 1;
    }
    if (std::strstr(commandLine, "--verify-player-controls")) {
        return RunPlayerControlTests() ? 0 : 1;
    }
    if (std::strstr(commandLine, "--verify-fear-worm-movements")) {
        return RunFearWormMovementTests() ? 0 : 1;
    }
    if (std::strstr(commandLine, "--verify-swarm-formations")) {
        return RunSwarmFormationTests() ? 0 : 1;
    }
    if (std::strstr(commandLine, "--verify-enemy-movements")) {
        return RunEnemyMovementTests() ? 0 : 1;
    }
    if (std::strstr(commandLine, "--verify-enemy-appearances")) {
        return RunEnemyAppearanceTests() ? 0 : 1;
    }
    if (std::strstr(commandLine, "--verify-boss-presentations")) {
        return RunBossPresentationTests() ? 0 : 1;
    }
    if (std::strstr(commandLine, "--verify-boss-spawn") || std::strstr(commandLine, "--verify-boss-factories")) {
        if (RunBossSpawnTests()) { return 0; }
        return 1;
    }
    if (std::strstr(commandLine, "--verify-enemy-debug-views")) {
        return RunEnemyDebugViewTests() ? 0 : 1;
    }
    if (std::strstr(commandLine, "--verify-jellyfish-attacks")) {
        return RunJellyfishAttackTests() ? 0 : 1;
    }
    if (std::strstr(commandLine, "--verify-beam-states")) {
        return RunBeamStateTests() ? 0 : 1;
    }
    if (std::strstr(commandLine, "--verify-title-states")) {
        return RunTitleStateTests() ? 0 : 1;
    }
    if (std::strstr(commandLine, "--verify-player-weapons")) {
        return RunPlayerWeaponPolymorphismTests() ? 0 : 1;
    }
#endif
    // 起動時の時間を計測するため、最初に現在時刻を取得する。
    const std::chrono::steady_clock::time_point startupBeginTime =std::chrono::steady_clock::now();

    StartupTiming::Begin(startupBeginTime);
    StartupSplash splash(instance);
    // ログ出力の初期化を行い、アプリケーション開始をログに残す。
    Logger::Initialize();
    // ログ出力の初期化を行い、アプリケーション開始をログに残す。
    Logger::Log("Application Start");

    D3DResourceLeakChecker leakChecker;

    Game game;

    Logger::Log("Game Initialize");
    {
        StartupTimingScope startupScope("Main.GameInitialize");
        game.Initialize(/*showWindow=*/false);
    }

    bool hasLoggedStartupTime = false;
    const bool isVerifyingOpenOcean =
        std::wstring(GetCommandLineW()).find(L"--verify-open-ocean") != std::wstring::npos;
    const bool isVerifyingUnderwaterAtmosphere =
        std::wstring(GetCommandLineW()).find(L"--verify-underwater-atmosphere") != std::wstring::npos;
    const bool isVerifyingAnemone =
        std::wstring(GetCommandLineW()).find(L"--verify-anemone") != std::wstring::npos;
    const bool isVerifyingDash =
        std::wstring(GetCommandLineW()).find(L"--verify-dash-enemy") != std::wstring::npos;
    const bool isVerifyingFullscreen =
        std::wstring(GetCommandLineW()).find(L"--verify-fullscreen") != std::wstring::npos;
    const bool isVerifyingLoopEnemy =
        std::wstring(GetCommandLineW()).find(L"--verify-loop-enemy") != std::wstring::npos;
    const bool isVerifyingCoilPerformance =
        std::wstring(GetCommandLineW()).find(L"--verify-coil-performance") != std::wstring::npos;
    int verificationFrameLimit = 300;
    if (isVerifyingLoopEnemy) { verificationFrameLimit = 900; }
    if (isVerifyingAnemone) { verificationFrameLimit = 360; }
    if (isVerifyingDash) { verificationFrameLimit = 480; }
    if (isVerifyingCoilPerformance) { verificationFrameLimit = 1200; }
    const bool verifyOceanTransition = isVerifyingOpenOcean || isVerifyingCoilPerformance || isVerifyingDash || isVerifyingAnemone || isVerifyingUnderwaterAtmosphere || isVerifyingFullscreen || isVerifyingLoopEnemy ||
        std::wstring(GetCommandLineW()).find(L"--verify-ocean-transition") != std::wstring::npos;
    int oceanVerificationFrame = 0;

    // ウィンドウの終了・ゲームからの終了要求のどちらでもループを抜ける。
    MSG msg {};
    while (msg.message != WM_QUIT) {

        if (WinApp::GetInstance()->ProcessMessage()) {
            Logger::Log("Window Close");
            break;
        }

        if (game.IsEndRequest()) {
            Logger::Log("Game End Request");
            break;
        }

        // 状態を更新してから描画することで、そのフレームの操作結果を画面へ反映する。
        try {
            if (!hasLoggedStartupTime) {
                {
                    StartupTimingScope startupScope("Main.FirstUpdate");
                    game.Update();
                }
                {
                    StartupTimingScope startupScope("Main.FirstDraw");
                    game.Draw();
                }
                splash.Close();
                game.ShowMainWindow();
            } else {
                game.Update();
                game.Draw();
            }
            if (isVerifyingFullscreen && (oceanVerificationFrame == 220 || oceanVerificationFrame == 245)) {
                const uint32_t descriptorCount = SrvManager::GetInstance()->GetAllocatedCount();
                Logger::Log("[FullscreenSmoke] Toggle display size");
                Logger::Flush();
                if (!SceneManager::GetInstance()->GetRenderer()->ToggleFullscreen()) {
                    throw std::runtime_error("Fullscreen verification display switch failed");
                }
                if (SrvManager::GetInstance()->GetAllocatedCount() != descriptorCount) {
                    throw std::runtime_error("Fullscreen verification changed descriptor allocation count");
                }
                Logger::Log("[FullscreenSmoke] PASS: descriptor count unchanged at " + std::to_string(descriptorCount));
            }
        } catch (const std::exception& error) {
            Logger::Error(std::string("Frame failed: ") + error.what());
            Logger::Flush();
            return 1;
        }
        if (verifyOceanTransition) {
            Logger::Flush();
            ++oceanVerificationFrame;
            if (oceanVerificationFrame == 60) {
                const std::string stageId = isVerifyingOpenOcean ? "stage04" : "stage01";
                Logger::Log("[OceanSmoke] Request title -> " + stageId + " transition");
                SceneManager::GetInstance()->SetNextScene(std::make_unique<LoadingScene>(std::make_unique<GamePlayScene>(stageId)));
            }
            if (oceanVerificationFrame == 180) {
                ScreenshotManager::GetInstance()->RequestCapture(true, false);
            }
            if (isVerifyingUnderwaterAtmosphere && oceanVerificationFrame == 270) {
                ScreenshotManager::GetInstance()->RequestCapture(true, false);
                Logger::Log("[UnderwaterSmoke] Capture underwater lighting");
            }
            if (isVerifyingLoopEnemy && (oceanVerificationFrame == 320 || oceanVerificationFrame == 440 || oceanVerificationFrame == 560)) {
                ScreenshotManager::GetInstance()->RequestCapture(true, false);
            }
            if (isVerifyingAnemone && (oceanVerificationFrame == 180 || oceanVerificationFrame == 240 || oceanVerificationFrame == 300)) {
                ScreenshotManager::GetInstance()->RequestCapture(true, false);
                Logger::Log("[AnemoneSmoke] Capture enemy and fan bullets");
            }
            if (isVerifyingCoilPerformance && (oceanVerificationFrame == 620 || oceanVerificationFrame == 750)) {
                ScreenshotManager::GetInstance()->RequestCapture(true, false);
                Logger::Log("[CoilPerformance] Capture boss appearance");
            }
            if (oceanVerificationFrame == verificationFrameLimit) {
                Logger::Log(std::string("[OceanSmoke] Completed: title -> ") +
                    (isVerifyingOpenOcean ? "stage04" : "stage01") + " transition check");
                break;
            }
        }

        if (!hasLoggedStartupTime) {
            const long long startupMilliseconds =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - startupBeginTime)
                    .count();

            Logger::Log(
                "[StartupTime] Application start to first frame presented: " +
                std::to_string(startupMilliseconds) + " ms");
            hasLoggedStartupTime = true;
            StartupTiming::Finish();
            if (StartupTiming::ShouldExitAfterStartup()) {
                break;
            }
        }
    }

    Logger::Log("Game Finalize");
    game.Finalize();

    Logger::Log("Application End");
    Logger::Finalize();

    return 0;
}

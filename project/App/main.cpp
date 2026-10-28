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

// 起動時の準備を行い、入力・更新・描画を繰り返した後にゲームを終了する。
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR commandLine, int)
{
// デバッグ用の検証指定がある場合は、通常のゲーム起動より先に実行する。
#ifdef _DEBUG
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
    const std::chrono::steady_clock::time_point startupBeginTime =
        std::chrono::steady_clock::now();

    StartupTiming::Begin(startupBeginTime);
    Logger::Initialize();
    Logger::Log("Application Start");

    D3DResourceLeakChecker leakChecker;

    Game game;

    Logger::Log("Game Initialize");
    {
        StartupTimingScope startupScope("Main.GameInitialize");
        game.Initialize();
    }

    bool hasLoggedStartupTime = false;
    const bool isVerifyingUnderwaterAtmosphere =
        std::wstring(GetCommandLineW()).find(L"--verify-underwater-atmosphere") != std::wstring::npos;
    const bool isVerifyingFullscreen =
        std::wstring(GetCommandLineW()).find(L"--verify-fullscreen") != std::wstring::npos;
    const bool isVerifyingLoopEnemy =
        std::wstring(GetCommandLineW()).find(L"--verify-loop-enemy") != std::wstring::npos;
    int verificationFrameLimit = 300;
    if (isVerifyingLoopEnemy) { verificationFrameLimit = 900; }
    const bool verifyOceanTransition = isVerifyingUnderwaterAtmosphere || isVerifyingFullscreen || isVerifyingLoopEnemy ||
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
            if (!verifyOceanTransition) throw;
            return 1;
        }
        if (verifyOceanTransition) {
            Logger::Flush();
            ++oceanVerificationFrame;
            if (oceanVerificationFrame == 60) {
                Logger::Log("[OceanSmoke] Request title -> stage01 transition");
                SceneManager::GetInstance()->SetNextScene(std::make_unique<LoadingScene>(std::make_unique<GamePlayScene>("stage01")));
            }
            if (isVerifyingUnderwaterAtmosphere && oceanVerificationFrame == 270) {
                ScreenshotManager::GetInstance()->RequestCapture(true, false);
                Logger::Log("[UnderwaterSmoke] Capture underwater lighting");
            }
            if (isVerifyingLoopEnemy && (oceanVerificationFrame == 320 || oceanVerificationFrame == 440 || oceanVerificationFrame == 560)) {
                ScreenshotManager::GetInstance()->RequestCapture(true, false);
            }
            if (oceanVerificationFrame == verificationFrameLimit) {
                Logger::Log("[OceanSmoke] PASS: title -> stage01 initialization after loading and rendered gameplay");
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

// アプリの入口。終了要求を受けるまでゲームの更新と描画を繰り返す。
#include "Engine/ImGuiManager/ImGuiManager.h"
#include "Engine/Logger/Logger.h"
#include "App/Scene/Application/Game.h"

#include <chrono>
#ifdef _DEBUG
#include "Tests/PlayerWeaponPolymorphismTests.h"
#include "Tests/JellyfishAttackPatternTests.h"
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
#include "Tests/PlayerControlTests.h"
#include <cstring>
#endif

// 起動時の準備を行い、入力・更新・描画を繰り返した後にゲームを終了する。
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR commandLine, int)
{
// デバッグ用の検証指定がある場合は、通常のゲーム起動より先に実行する。
#ifdef _DEBUG
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
    if (std::strstr(commandLine, "--verify-jellyfish-patterns")) {
        return RunJellyfishAttackPatternTests() ? 0 : 1;
    }
    if (std::strstr(commandLine, "--verify-player-weapons")) {
        return RunPlayerWeaponPolymorphismTests() ? 0 : 1;
    }
#endif
    const std::chrono::steady_clock::time_point startupBeginTime =
        std::chrono::steady_clock::now();

    Logger::Initialize();
    Logger::Log("Application Start");

    D3DResourceLeakChecker leakChecker;

    Game game;

    Logger::Log("Game Initialize");
    game.Initialize();

    bool hasLoggedStartupTime = false;

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
        game.Update();
        game.Draw();

        if (!hasLoggedStartupTime) {
            const long long startupMilliseconds =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - startupBeginTime)
                    .count();

            Logger::Log(
                "[StartupTime] Application start to first frame presented: " +
                std::to_string(startupMilliseconds) + " ms");
            hasLoggedStartupTime = true;
        }
    }

    Logger::Log("Game Finalize");
    game.Finalize();

    Logger::Log("Application End");
    Logger::Finalize();

    return 0;
}

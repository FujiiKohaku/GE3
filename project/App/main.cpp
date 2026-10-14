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
#include "Tests/BossFactoryTests.h"
#include "Tests/BossPresentationTests.h"
#include "Tests/EnemyAppearanceTests.h"
#include "Tests/EnemyMovementTests.h"
#include "Tests/SwarmFormationTests.h"
#include "Tests/FearWormMovementTests.h"
#include "Tests/PlayerControlTests.h"
#include <cstring>
#endif

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR commandLine, int)
{
#ifdef _DEBUG
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
    if (std::strstr(commandLine, "--verify-boss-factories")) {
        return RunBossFactoryTests() ? 0 : 1;
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

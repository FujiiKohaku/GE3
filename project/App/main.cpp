#include "Engine/ImGuiManager/ImGuiManager.h"
#include "Engine/Logger/Logger.h"
#include "Scene/Game.h"

#ifdef _DEBUG
#include <chrono>
#endif

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
#ifdef _DEBUG
    const std::chrono::steady_clock::time_point startupBeginTime =
        std::chrono::steady_clock::now();
#endif

    Logger::Initialize();
    Logger::Log("Application Start");

    D3DResourceLeakChecker leakChecker;

    Game game;

    Logger::Log("Game Initialize");
    game.Initialize();

#ifdef _DEBUG
    const std::chrono::steady_clock::time_point startupEndTime =
        std::chrono::steady_clock::now();
    const long long startupMilliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            startupEndTime - startupBeginTime)
            .count();

    Logger::Log(
        "[StartupTime] Application start to Game::Initialize complete: " +
        std::to_string(startupMilliseconds) + " ms");
#endif

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
    }

    Logger::Log("Game Finalize");
    game.Finalize();

    Logger::Log("Application End");
    Logger::Finalize();

    return 0;
}

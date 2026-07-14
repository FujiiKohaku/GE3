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
    bool hasLoggedStartupTime = false;
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

#ifdef _DEBUG
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
#endif
    }

    Logger::Log("Game Finalize");
    game.Finalize();

    Logger::Log("Application End");
    Logger::Finalize();

    return 0;
}

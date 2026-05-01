#include "Engine/ImGuiManager/ImGuiManager.h"
#include "Engine/Logger/Logger.h"
#include "Scene/Game.h"

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    Logger::Initialize();
    Logger::Log("Application Start");

    D3DResourceLeakChecker leakChecker;

    Game game;

    Logger::Log("Game Initialize");
    game.Initialize();

    MSG msg {};
    while (msg.message != WM_QUIT) {

        if (game.GetWinApp()->ProcessMessage()) {
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
#include "Engine/ImGuiManager/ImGuiManager.h"
#include "Engine/Logger/Logger.h"
#include "Scene/Game.h"

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    Logger::Initialize();
    Logger::Log("Application Start");

    D3DResourceLeakChecker leakChecker;

    Game game;
    game.Initialize();

    MSG msg {};
    while (msg.message != WM_QUIT) {

        if (game.GetWinApp()->ProcessMessage()) {
            break;
        }

        if (game.IsEndRequest()) {
            break;
        }

        game.Update();
        game.Draw();
    }

    game.Finalize();

    Logger::Log("Application End");
    Logger::Finalize();

    return 0;
}
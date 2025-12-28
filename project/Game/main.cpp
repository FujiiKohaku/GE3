
#include "ImGuiManager.h"
#include "Game.h"



// ======================= エントリーポイント =====================
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    // ここで作る → main の終わりで確実に実行される
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
    return 0;
}

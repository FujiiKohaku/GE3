
#include "ImGuiManager.h"
#include "Scene/Game.h"

// ======================= ImGui用ウィンドウプロシージャ =====================
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

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

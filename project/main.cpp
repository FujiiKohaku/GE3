
#include "ImGuiManager.h"
#include "Scene/Game.h"

// ======================= ImGui用ウィンドウプロシージャ =====================
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

// ======================= エントリーポイント =====================
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{

    // -------------------------------
    // ゲーム本体初期化
    // -------------------------------
    Game* game = new Game();
    game->Initialize();

    //  メインループ
    MSG msg {};
    while (msg.message != WM_QUIT) {

        if (game->GetWinApp()->ProcessMessage()) {
            break;
        }

        if (game->IsEndRequest()) {
            break;
        }

        game->Update();

        game->Draw();
    }

    //  終了処理
    game->Finalize();

    delete game;

    return 0;
}

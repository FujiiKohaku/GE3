#include "Scene/Game.h"

// ======================= ImGui用ウィンドウプロシージャ =====================
// ImGuiの入力処理をWindowsメッセージから受け取るための宣言（ImGui内部用）
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

// ======================= エントリーポイント =====================
// Windowsアプリのメイン関数（プログラムのスタート地点）
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    // -------------------------------
    // 1. DirectXリソースリーク検出準備
    // ------------------------------- 
    D3DResourceLeakChecker leakChecker; // 終了時にリソースリークを自動検出 
     
    // -------------------------------
    // 2. ゲーム本体の生成と初期化
    // -------------------------------
    Game* game = new Game(); // ゲームクラスのインスタンス作成
    game->Initialize(); // 初期化処理（ウィンドウ・DX・各マネージャー）

    // -------------------------------
    // 3. メインループ（ウィンドウが閉じるまで）
    // -------------------------------
    MSG msg {};
    while (msg.message != WM_QUIT) {

        // Windowsのシステムメッセージを処理
        if (game->GetWinApp()->ProcessMessage()) {
            break; // ウィンドウが閉じられたらループ終了
        }

        // ゲーム本体の更新と描画
        game->Update(); // 毎フレームの更新（入力・カメラ・オブジェクトなど）
        game->Draw(); // 毎フレームの描画（3D・2D・UI）
    }

    // -------------------------------
    // 4. 終了処理（解放）
    // -------------------------------
    game->Finalize(); // 解放処理（リソース破棄・サウンド・ImGuiなど）
    delete game; // メモリ解放

    return 0; // 正常終了
}

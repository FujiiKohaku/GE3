#include "DirectXCommon.h"
#include "ImGuiManager.h"
#include "Scene/Game.h"
#include "SrvManager.h"
#include "TextureManager.h"
#include "WinApp.h"
// ======================= ImGui用ウィンドウプロシージャ =====================
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

// ======================= エントリーポイント =====================
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    // -------------------------------
    // 例外キャッチ設定
    // -------------------------------
    SetUnhandledExceptionFilter(Utility::ExportDump);
    std::filesystem::create_directory("logs");

    // ==============================
    // 1. 基本システム初期化
    // ==============================
    WinApp* winApp = new WinApp();
    winApp->initialize();

    DirectXCommon* dxCommon = new DirectXCommon();
    dxCommon->Initialize(winApp);

    SrvManager* srvManager = new SrvManager();
    srvManager->Initialize(dxCommon);

    // ==============================
    // 2. 各マネージャ初期化
    // ==============================
    TextureManager::GetInstance()->Initialize(dxCommon, srvManager);
    

    // -------------------------------
    // リーク検出
    // -------------------------------
    D3DResourceLeakChecker leakChecker;

    // -------------------------------
    // Imgui初期化
    // -------------------------------
    ImGuiManager* imguiManager = new ImGuiManager();
    imguiManager->Initialize(winApp, dxCommon, srvManager);

    // -------------------------------
    // ゲーム本体初期化
    // -------------------------------
    Game* game = new Game();
    game->Initialize(winApp, dxCommon, srvManager);


   
    // ==============================
    // 3. メインループ
    // ==============================
    MSG msg {};
    while (msg.message != WM_QUIT) {

        if (winApp->ProcessMessage()) {
            break;
        }

        // ======== ImGui開始 ========
        imguiManager->Begin();

        // ======== 更新 ========
        game->Update();

        // ======== ImGui終了 ========
        imguiManager->End();

        // ======== 描画 ============
        srvManager->PreDraw();

        dxCommon->PreDraw(); // ← GAMEの中じゃなくここに
        game->Draw();

        imguiManager->Draw(); // 必ず最後

        dxCommon->PostDraw();
    }

    // ==============================
    // 4. 終了処理
    // ==============================
    game->Finalize();
    TextureManager::GetInstance()->Finalize();
    
    imguiManager->Finalize();
    delete imguiManager;
    delete srvManager;
    delete game;
    delete dxCommon;
    delete winApp;

    return 0;
}

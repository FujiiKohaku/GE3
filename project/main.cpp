#include "DirectXCommon.h"
#include "ParticleManager.h"
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
    ParticleManager::GetInstance()->Initialize(dxCommon, srvManager);

    // -------------------------------
    // リーク検出
    // -------------------------------
    D3DResourceLeakChecker leakChecker;

    // -------------------------------
    // ゲーム本体初期化
    // -------------------------------
    Game* game = new Game();
    game->Initialize(winApp, dxCommon);

    // ==============================
    // 3. メインループ
    // ============================== 
    MSG msg {};
    while (msg.message != WM_QUIT) {

        if (winApp->ProcessMessage()) {
            break;
        }

        game->Update();

        // SRVヒープセットはParticleManagerを使う前に
        srvManager->PreDraw();

        game->Draw();
    }

    // ==============================
    // 4. 終了処理
    // ==============================
    game->Finalize();
    TextureManager::GetInstance()->Finalize();
    ParticleManager::GetInstance()->Finalize();

    delete srvManager;
    delete game;
    delete dxCommon;
    delete winApp;

    return 0;
}

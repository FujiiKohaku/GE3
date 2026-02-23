#include "Game.h"
#include <numbers>

void Game::Initialize()
{
    // -------------------------------
    // 例外キャッチ設定（アプリ起動時に1回だけ）
    // -------------------------------
    SetUnhandledExceptionFilter(Utility::ExportDump);
    std::filesystem::create_directory("logs");
    winApp_ = std::make_unique<WinApp>();
    winApp_->initialize();
    DirectXCommon::GetInstance()->Initialize(winApp_.get());
    SrvManager::GetInstance()->Initialize(DirectXCommon::GetInstance());
    TextureManager::GetInstance()->Initialize(DirectXCommon::GetInstance(), SrvManager::GetInstance());
    ImGuiManager::GetInstance()->Initialize(winApp_.get(), DirectXCommon::GetInstance(), SrvManager::GetInstance());
    SpriteManager::GetInstance()->Initialize(DirectXCommon::GetInstance());
    ModelManager::GetInstance()->Initialize(DirectXCommon::GetInstance());
    Object3dManager::GetInstance()->Initialize(DirectXCommon::GetInstance());
    SkinningObject3dManager::GetInstance()->Initialize(DirectXCommon::GetInstance());
    modelCommon_.Initialize(DirectXCommon::GetInstance());
    // 入力関連
    Input::GetInstance()->Initialize(winApp_.get());
    // パーティクル関連
    // ParticleManager::GetInstance()->Initialize(DirectXCommon::GetInstance(), SrvManager::GetInstance(), camera_);
    ModelManager::GetInstance()->Load("plane.obj");
    ModelManager::GetInstance()->Load("axis.obj");
    ModelManager::GetInstance()->Load("titleTex.obj");
    ModelManager::GetInstance()->Load("fence.obj");

    TextureManager::GetInstance()->LoadTexture("resources/white.png");
    TextureManager::GetInstance()->LoadTexture("resources/uvChecker.png");
    TextureManager::GetInstance()->LoadTexture("resources/fence.png");
    // LoadTexture
    TextureManager::GetInstance()->LoadTexture("resources/BaseColor_Cube.png");
    BaseScene* scene = new TitleScene();
    // シーンマネージャーに最初のシーンをセット
    SceneManager::GetInstance()->SetNextScene(std::make_unique<TitleScene>());

    // サウンド関連
    SoundManager::GetInstance()->Initialize();
}

void Game::Update()
{
    // ======== ImGui begin ========
    ImGuiManager::GetInstance()->Begin();

    // --- ゲーム更新 ---
    Input::GetInstance()->Update();
    // camera_->Update();

    // camera_->DebugUpdate();

    // エスケープで離脱
    if (Input::GetInstance()->IsKeyPressed(DIK_ESCAPE)) {
        endRequest_ = true;
    }

    SceneManager::GetInstance()->Update();

    SceneManager::GetInstance()->DrawImGui();

    // ======== ImGui end ========
    ImGuiManager::GetInstance()->End();
}

void Game::Draw()
{
    // === フレーム開始 ===
    SrvManager::GetInstance()->PreDraw();
    DirectXCommon::GetInstance()->PreDraw();

    SceneManager::GetInstance()->Draw3D();
    SceneManager::GetInstance()->Draw2D();

    // === フレーム終了 ===
    ImGuiManager::GetInstance()->Draw();
    DirectXCommon::GetInstance()->PostDraw();
}

void Game::Finalize()
{
    // 1. シーン停止
    SceneManager::GetInstance()->Finalize();

    // 2. ImGui（最優先で止める）
    ImGuiManager::GetInstance()->Finalize();

    // 3. 描画系
    SkinningObject3dManager::GetInstance()->Finalize();
    Object3dManager::GetInstance()->Finalize();
    SpriteManager::GetInstance()->Finalize();
    ModelManager::GetInstance()->Finalize();

    // 4. リソース
    TextureManager::GetInstance()->Finalize();
    SrvManager::GetInstance()->Finalize();

    // 5. サウンド
    SoundManager::GetInstance()->Finalize();

    // 6. DirectX（Device完全解放）
    DirectXCommon::GetInstance()->Finalize();

    // 7. ウィンドウ（最後）
    if (winApp_) {
        winApp_->Finalize();
        winApp_.reset();
    }

    // camera_ も unique_ptr なら delete 不要
}

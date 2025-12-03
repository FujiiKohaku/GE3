#include "Game.h"
#include <numbers>

void Game::Initialize()
{
    // -------------------------------
    // 例外キャッチ設定（アプリ起動時に1回だけ）
    // -------------------------------
    SetUnhandledExceptionFilter(Utility::ExportDump);
    std::filesystem::create_directory("logs");
    winApp_ = new WinApp();
    winApp_->initialize();
    DirectXCommon::GetInstance()->Initialize(winApp_);
    SrvManager::GetInstance()->Initialize(DirectXCommon::GetInstance());
    TextureManager::GetInstance()->Initialize(DirectXCommon::GetInstance(), SrvManager::GetInstance());
    ImGuiManager::GetInstance()->Initialize(winApp_, DirectXCommon::GetInstance(), SrvManager::GetInstance());
    SpriteManager::GetInstance()->Initialize(DirectXCommon::GetInstance());
    ModelManager::GetInstance()->initialize(DirectXCommon::GetInstance());
    Object3dManager::GetInstance()->Initialize(DirectXCommon::GetInstance());
    camera_ = new Camera();
    camera_->SetTranslate({ 0.0f, 0.0f, 2.0f });
    Object3dManager::GetInstance()->SetDefaultCamera(camera_);
    modelCommon_.Initialize(DirectXCommon::GetInstance());
    // 入力関連
    Input::GetInstance()->Initialize(winApp_);
    // パーティクル関連
    ParticleManager::GetInstance()->Initialize(DirectXCommon::GetInstance(), SrvManager::GetInstance(), camera_);
    ModelManager::GetInstance()->LoadModel("plane.obj");
    ModelManager::GetInstance()->LoadModel("axis.obj");
    ModelManager::GetInstance()->LoadModel("titleTex.obj");
    ModelManager::GetInstance()->LoadModel("fence.obj");
    TextureManager::GetInstance()->LoadTexture("resources/uvChecker.png");
    TextureManager::GetInstance()->LoadTexture("resources/fence.png");

    BaseScene* scene = new TitleScene();
    // シーンマネージャーに最初のシーンをセット
    SceneManager::GetInstance()->SetNextScene(scene);
}

void Game::Update()
{
    // ======== ImGui begin ========
    ImGuiManager::GetInstance()->Begin();

    // --- ゲーム更新 ---
    Input::GetInstance()->Update();
    camera_->Update();
  
    camera_->DebugUpdate();

    // エスケープで離脱
    if (Input::GetInstance()->IsKeyPressed(DIK_ESCAPE)) {
        endRequest_ = true;
    }

    SceneManager::GetInstance()->Update();

    // ======== ImGui end ========
    ImGuiManager::GetInstance()->End();
}

void Game::Draw()
{
    // === フレーム開始 ===
    SrvManager::GetInstance()->PreDraw();
    DirectXCommon::GetInstance()->PreDraw();

    // === シーンに渡す ===
    SceneManager::GetInstance()->Draw();

    // === フレーム終了 ===
    ImGuiManager::GetInstance()->Draw();
    DirectXCommon::GetInstance()->PostDraw();
}

void Game::Finalize()
{
    // シーンマネージャーも singleton
    ParticleManager::GetInstance()->Finalize();
    Object3dManager::GetInstance()->Finalize();
    SpriteManager::GetInstance()->Finalize();
    ModelManager::GetInstance()->Finalize();
    TextureManager::GetInstance()->Finalize();
    ImGuiManager::GetInstance()->Finalize();
    SrvManager::GetInstance()->Finalize();
    // DirectXCommonはFinalizeしてもデバイス破棄処理だけ。deleteは不要
    DirectXCommon::GetInstance()->Finalize();

    winApp_->Finalize();
    delete winApp_;
    delete camera_;
}

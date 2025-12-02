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
    // TextureManager::GetInstance()->LoadTexture("resources/circle.png")

    ModelManager::GetInstance()->initialize(DirectXCommon::GetInstance());
    Object3dManager::GetInstance()->Initialize(DirectXCommon::GetInstance());
    camera_ = new Camera();
    camera_->SetTranslate({ 0.0f, 0.0f, 2.0f });
    Object3dManager::GetInstance()->SetDefaultCamera(camera_);
    modelCommon_.Initialize(DirectXCommon::GetInstance());
    // 入力関連
    input_ = new Input();
    input_->Initialize(winApp_);
    // パーティクル関連
    ParticleManager::GetInstance()->Initialize(DirectXCommon::GetInstance(), SrvManager::GetInstance(), camera_);
    scene_ = new GamePlayScene();
    scene_->Initialize();
}

void Game::Update()
{
    // ======== ImGui begin ========
    ImGuiManager::GetInstance()->Begin();

    // --- ゲーム更新 ---
    input_->Update();
    camera_->Update();
    ParticleManager::GetInstance()->Update();
    camera_->DebugUpdate();

    // エスケープで離脱
    if (input_->IsKeyPressed(DIK_ESCAPE)) {
        endRequest_ = true;
    }

    scene_->Update();

    // ======== ImGui end ========
    ImGuiManager::GetInstance()->End();
}

void Game::Draw()
{
    // === フレーム開始 ===
    SrvManager::GetInstance()->PreDraw();
    DirectXCommon::GetInstance()->PreDraw();

    // === シーンに渡す ===
    scene_->Draw3D(); // モデル + パーティクル
    scene_->Draw2D(); // スプライト
    scene_->DrawImGui(); // debug UI

    // === フレーム終了 ===
    ImGuiManager::GetInstance()->Draw();
    DirectXCommon::GetInstance()->PostDraw();
}

void Game::Finalize()
{
    delete scene_;
    delete camera_;
    delete input_;

    ModelManager::GetInstance()->Finalize();
    TextureManager::GetInstance()->Finalize();

    ImGuiManager::GetInstance()->Finalize();

    winApp_->Finalize();
    delete winApp_;
}

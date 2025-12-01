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
    dxCommon_ = new DirectXCommon();
    dxCommon_->Initialize(winApp_);
    srvManager_ = new SrvManager();
    srvManager_->Initialize(dxCommon_);
    TextureManager::GetInstance()->Initialize(dxCommon_, srvManager_);

    imguiManager_ = new ImGuiManager();
    imguiManager_->Initialize(winApp_, dxCommon_, srvManager_);

// Sprite関連
    spriteManager_ = new SpriteManager();
    spriteManager_->Initialize(dxCommon_);

    TextureManager::GetInstance()->LoadTexture("resources/uvChecker.png");
    // TextureManager::GetInstance()->LoadTexture("resources/circle.png");
    sprite_ = new Sprite();
    sprite_->Initialize(spriteManager_, "resources/uvChecker.png");
    sprite_->SetPosition({ 100.0f, 100.0f });


// 3D関連
    object3dManager_ = new Object3dManager();
    object3dManager_->Initialize(dxCommon_);

    camera_ = new Camera();
    camera_->SetTranslate({ 0.0f, 0.0f, 2.0f });
    object3dManager_->SetDefaultCamera(camera_);

    modelCommon_.Initialize(dxCommon_);

    ModelManager::GetInstance()->initialize(dxCommon_);
    ModelManager::GetInstance()->LoadModel("plane.obj");
    ModelManager::GetInstance()->LoadModel("axis.obj");
    ModelManager::GetInstance()->LoadModel("titleTex.obj");
    ModelManager::GetInstance()->LoadModel("fence.obj");
    player2_.Initialize(object3dManager_);
    player2_.SetModel("fence.obj");
    player2_.SetTranslate({ 3.0f, 0.0f, 10.0f });
    player2_.SetRotate({ std::numbers::pi_v<float> / 2.0f, std::numbers::pi_v<float>, 0.0f });


// 入力関連
    input_ = new Input();
    input_->Initialize(winApp_);


// サウンド関連
    soundManager_.Initialize();
    bgm = soundManager_.SoundLoadWave("Resources/BGM.wav");


// パーティクル関連
    particleManager_ = new ParticleManager();
    particleManager_->Initialize(dxCommon_, srvManager_, camera_);


}

void Game::Update()
{
    // ======== ImGui begin ========
    imguiManager_->Begin();

    // --- ゲーム更新 ---
    input_->Update();
    player2_.Update();
    camera_->Update();
    sprite_->Update();
    particleManager_->Update();
    camera_->DebugUpdate();

    //エスケープで離脱
    if (input_->IsKeyPressed(DIK_ESCAPE)) {
        endRequest_ = true;
    }

#ifdef USE_IMGUI
    // ----------------------
    // ここに ImGui GUI 書く
    // ----------------------
    ImGui::Begin("Player2 Debug");

    Vector3 pos = player2_.GetTranslate();
    if (ImGui::DragFloat3("Position", &pos.x, 0.1f)) {
        player2_.SetTranslate(pos);
    }

    Vector3 rot = player2_.GetRotate();
    if (ImGui::DragFloat3("Rotate", &rot.x, 0.01f)) {
        player2_.SetRotate(rot);
    }

    Vector3 scale = player2_.GetScale();
    if (ImGui::DragFloat3("Scale", &scale.x, 0.01f)) {
        player2_.SetScale(scale);
    }

    ImGui::End();
#endif

    // ======== ImGui end ========
    imguiManager_->End();



}

void Game::Draw()
{
    // ========== 3D / パーティクル ==========

    srvManager_->PreDraw(); //  SRV（DX12 全体の前処理）
    dxCommon_->PreDraw(); //  DirectX のレンダー開始

    {
        object3dManager_->PreDraw();
        particleManager_->PreDraw();
        particleManager_->Draw();
    }

    // ========== 2D（Sprite） ==========
    {
        spriteManager_->PreDraw();
        // sprite_->Draw();
    }

    // ========== ImGui ==========
    imguiManager_->Draw();

    // ========== DirectX 終了処理 ==========
    dxCommon_->PostDraw(); 
}

void Game::Finalize()
{
    // ＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝
    //  ① ゲーム内オブジェクトの破棄
    // ＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝
    delete particleManager_;
    delete object3dManager_;
    delete spriteManager_;
    delete sprite_;
    delete camera_;
    delete input_;

    // ＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝
    //  ② マネージャ系（DirectXに依存するもの）
    // ＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝
    ModelManager::GetInstance()->Finalize();
    TextureManager::GetInstance()->Finalize();
    soundManager_.Finalize(&bgm);

    // ＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝
    //  ③ ImGui（DirectX/WinApp依存）
    // ＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝
    imguiManager_->Finalize();
    delete imguiManager_;

    // ＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝
    //  ④ DirectX 関連
    // ＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝
    delete srvManager_;
    delete dxCommon_;

    // ＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝
    //  ⑤ 最後に WinApp
    // ＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝
    winApp_->Finalize();
    delete winApp_;
}

#include "Game.h"
#include <numbers>

void Game::Initialize(WinApp* winApp, DirectXCommon* dxCommon)
{
    // ==============================
    //  基本初期化
    // ==============================
    winApp_ = winApp;
    dxCommon_ = dxCommon;

#pragma region Sprite関連

    // --- SpriteManager ---
    spriteManager_ = new SpriteManager();
    spriteManager_->Initialize(dxCommon_);

    // --- テクスチャ読み込み ---
    TextureManager::GetInstance()->LoadTexture("resources/uvChecker.png");

    // --- スプライト生成 ---
    sprite_ = new Sprite();
    sprite_->Initialize(spriteManager_, "resources/uvChecker.png");

#pragma endregion

#pragma region 3D関連

    // --- Object3dManager ---
    object3dManager_ = new Object3dManager();
    object3dManager_->Initialize(dxCommon_);

    // --- カメラ ---
    camera_ = new Camera();
    camera_->SetTranslate({ 0.0f, 0.0f, -10.0f });
    object3dManager_->SetDefaultCamera(camera_);

    // --- モデル共通設定 ---
    modelCommon_.Initialize(dxCommon_);

    // --- モデル読み込み ---
    ModelManager::GetInstance()->initialize(dxCommon_);
    ModelManager::GetInstance()->LoadModel("plane.obj");
    ModelManager::GetInstance()->LoadModel("axis.obj");
    ModelManager::GetInstance()->LoadModel("titleTex.obj");

    // --- 3Dオブジェクト生成 ---
    player2_.Initialize(object3dManager_);
    player2_.SetModel("titleTex.obj");
    player2_.SetTranslate({ 3.0f, 0.0f, 0.0f });
    player2_.SetRotate({ std::numbers::pi_v<float> / 2.0f, std::numbers::pi_v<float>,0.0f });

#pragma endregion

#pragma region 入力関連

    input_ = new Input();
    input_->Initialize(winApp_);

#pragma endregion

#pragma region サウンド関連

    soundManager_.Initialize();
    bgm = soundManager_.SoundLoadWave("Resources/BGM.wav");

#pragma endregion
}

void Game::Update()
{
    // ==============================
    //  ImGuiフレーム開始
    // ==============================
 /*   ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();*/

    // --- 開発用UI ---
 /*   ImGui::Begin("Camera Controller");
    ImGui::SliderFloat3("Translate", &camera_->GetTranslate().x, -50.0f, 50.0f);
    ImGui::SliderFloat3("Rotate", &camera_->GetRotate().x, -3.14f, 3.14f);
    ImGui::End();

    ImGui::Render();*/

    // ==============================
    //  更新処理
    // ==============================
    input_->Update();
    player2_.Update();
    camera_->Update();
    sprite_->Update();
}

void Game::Draw()
{
    // ===== 3D描画 =====
    dxCommon_->PreDraw();
  
    object3dManager_->PreDraw();
    player2_.Draw();

    // ===== 2D描画（スプライト） =====
    spriteManager_->PreDraw();
    sprite_->Draw();

    //// ===== ImGui描画 =====
    //ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), dxCommon_->GetCommandList());

    // ===== フレーム終了 =====
    dxCommon_->PostDraw();
}

void Game::Finalize()
{
    // ==============================
    //  リソース解放
    // ==============================

    // --- 描画系 ---
    delete object3dManager_;
    sprites_.clear();
    delete spriteManager_;
    delete sprite_;
    delete camera_;

    // --- 入力 ---
    delete input_;

    //// --- ImGui ---
    //ImGui_ImplDX12_Shutdown();
    //ImGui_ImplWin32_Shutdown();
    //ImGui::DestroyContext();

    // --- モデル・サウンド ---
    ModelManager::GetInstance()->Finalize();
    soundManager_.Finalize(&bgm);
}

#include "Game.h"
#include <numbers>
void Game::Initialize(WinApp* winApp, DirectXCommon* dxCommon)
{
    winApp_ = winApp;
    dxCommon_ = dxCommon;

#pragma region object sprite

    // SpriteManager
    spriteManager_ = new SpriteManager();
    spriteManager_->Initialize(dxCommon_);
    TextureManager::GetInstance()->LoadTexture("resources/uvChecker.png");

    sprite_ = new Sprite();
    sprite_->Initialize(spriteManager_, "resources/uvChecker.png");

    // =============================
    // 3. 3D関連の初期化
    // =============================

    // Object3dManager
    object3dManager_ = new Object3dManager();
    object3dManager_->Initialize(dxCommon_);

    // カメラ
    camera_ = new Camera();
    camera_->SetTranslate({ 0.0f, 0.0f, -10.0f });
    object3dManager_->SetDefaultCamera(camera_);
    // モデル共通設定

    modelCommon_.Initialize(dxCommon_);

    ModelManager::GetInstance()->initialize(dxCommon_);
    ModelManager::GetInstance()->LoadModel("plane.obj");
    ModelManager::GetInstance()->LoadModel("axis.obj");
    ModelManager::GetInstance()->LoadModel("titleTex.obj");
    // =============================
    // 4. モデルと3Dオブジェクト生成
    // =============================

    // プレイヤー

    player2_.Initialize(object3dManager_);
    player2_.SetModel("titleTex.obj");
    player2_.SetTranslate({ 3.0f, 0.0f, 0.0f }); // 右に移動
    player2_.SetRotate({ std::numbers::pi_v<float> / 2.0f, std::numbers::pi_v<float>, 0.0f });
    // 敵

#pragma endregion

    //=================================
    // キーボードインスタンス作成
    //=================================

    input_ = new Input();
    //=================================
    // キーボード情報の取得開始
    //=================================
    input_->Initialize(winApp_);

    //=================================
    // サウンドマネージャーインスタンス作成
    //=================================

    // サウンドマネージャー初期化！
    soundManager_.Initialize();
    // サウンドファイルを読み込み（パスはプロジェクトに合わせて調整）
    bgm = soundManager_.SoundLoadWave("Resources/BGM.wav");
}

void Game::Update()
{
    // ==============================
    //  フレームの先頭処理
    // ==============================
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // ==============================
    //  開発用UI
    // ==============================

    // ==============================
    // ImGui更新（UI構築）
    // ==============================
    ImGui::Begin("Camera Controller");
    ImGui::SliderFloat3("Translate", &camera_->GetTranslate().x, -50.0f, 50.0f);
    ImGui::SliderFloat3("Rotate", &camera_->GetRotate().x, -3.14f, 3.14f);

    ImGui::End();

    ImGui::Render(); // ImGuiの内部コマンドを生成（描画直前に呼ぶ）

    // ==============================
    //  更新処理（Update）
    // ==============================
    // 入力状態の更新
    input_->Update();

    //============================================

    //============================================

    // 各3Dオブジェクトの更新

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

    // ===== ImGui（最前面） =====
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), dxCommon_->GetCommandList());

    // ===== 終了 =====
    dxCommon_->PostDraw();
}

void Game::Finalize()
{
    // 1. 描画系（Object/Sprite）を先に破棄
    delete object3dManager_;
    sprites_.clear();
    delete spriteManager_;
    delete camera_; // ←追加
    delete input_;
    delete sprite_;
    //  2. ImGuiを破棄（DirectXがまだ生きているうちに）
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    // 3. モデルやテクスチャを解放
    ModelManager::GetInstance()->Finalize();

    // 4. サウンドを解放
    soundManager_.Finalize(&bgm);
}
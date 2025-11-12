#include "Game.h"
#include "ParticleEmitter.h"
#include "ParticleManager.h"
#include <numbers>

void Game::Initialize(WinApp* winApp, DirectXCommon* dxCommon)
{
    winApp_ = winApp;
    dxCommon_ = dxCommon;

#pragma region Sprite関連
    spriteManager_ = new SpriteManager();
    spriteManager_->Initialize(dxCommon_);

    TextureManager::GetInstance()->LoadTexture("resources/uvChecker.png");

    sprite_ = new Sprite();
    sprite_->Initialize(spriteManager_, "resources/uvChecker.png");
#pragma endregion

#pragma region 3D関連
    object3dManager_ = new Object3dManager();
    object3dManager_->Initialize(dxCommon_);

    camera_ = new Camera();
    camera_->SetTranslate({ 0.0f, 0.0f, -10.0f });
    object3dManager_->SetDefaultCamera(camera_);

    modelCommon_.Initialize(dxCommon_);

    ModelManager::GetInstance()->initialize(dxCommon_);
    ModelManager::GetInstance()->LoadModel("plane.obj");
    ModelManager::GetInstance()->LoadModel("axis.obj");
    ModelManager::GetInstance()->LoadModel("titleTex.obj");

    player2_.Initialize(object3dManager_);
    player2_.SetModel("titleTex.obj");
    player2_.SetTranslate({ 3.0f, 0.0f, 0.0f });
    player2_.SetRotate({ std::numbers::pi_v<float> / 2.0f, std::numbers::pi_v<float>, 0.0f });
#pragma endregion

#pragma region 入力関連
    input_ = new Input();
    input_->Initialize(winApp_);
#pragma endregion

#pragma region サウンド関連
    soundManager_.Initialize();
    bgm = soundManager_.SoundLoadWave("Resources/BGM.wav");
#pragma endregion

#pragma region パーティクル関連
    // ① パーティクルグループ登録
    ParticleManager::GetInstance()->CreateParticleGroup("UV", "resources/uvChecker.png");

    // ② エミッタ生成と設定
    emitter_ = new ParticleEmitter();
    emitter_->SetGroupName("UV");
    emitter_->SetPosition({ 0, 5, 0 });
#pragma endregion
}

void Game::Update()
{
    input_->Update();
    player2_.Update();
    camera_->Update();
    sprite_->Update();

    // プレイヤーの位置を取得
    Vector3 pos = player2_.GetTranslate();

    // エミッタ位置をプレイヤーに追従
    emitter_->SetPosition(pos);

    // パーティクルを発生（1フレームごと）
    emitter_->Emit();

    // パーティクル全体更新
    ParticleManager::GetInstance()->Update();
}

void Game::Draw()
{
    dxCommon_->PreDraw();

    object3dManager_->PreDraw();
    player2_.Draw();

    spriteManager_->PreDraw();
    sprite_->Draw();

    // パーティクル描画
    ParticleManager::GetInstance()->Draw();

    dxCommon_->PostDraw();
} 

void Game::Finalize()
{
    delete object3dManager_;
    delete spriteManager_;
    delete sprite_;
    delete camera_;
    delete input_;
    delete emitter_; // ← 忘れず解放

    ModelManager::GetInstance()->Finalize();
    soundManager_.Finalize(&bgm);
}

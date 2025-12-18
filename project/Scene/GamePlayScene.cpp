#include "GamePlayScene.h"
#include "ParticleManager.h"
#include "SphereObject.h"
#include <numbers>
void GamePlayScene::Initialize()
{
    camera_ = new Camera();
    camera_->SetTranslate({ 0, 0, 0 });


    Object3dManager::GetInstance()->SetDefaultCamera(camera_);

    ParticleManager::GetInstance()->Initialize(DirectXCommon::GetInstance(), SrvManager::GetInstance(), camera_);

    Object3dManager::GetInstance()->SetDefaultCamera(camera_);

    sprite_ = new Sprite();
    sprite_->Initialize(SpriteManager::GetInstance(), "resources/uvChecker.png");
    sprite_->SetPosition({ 100.0f, 100.0f });
    // サウンド関連
    bgm = SoundManager::GetInstance()->SoundLoadWave("Resources/BGM.wav");
    player2_ = new Object3d();
    player2_->Initialize(Object3dManager::GetInstance());
    player2_->SetModel("fence.obj");
    player2_->SetTranslate({ 3.0f, 0.0f, 0.0f });
    player2_->SetRotate({ std::numbers::pi_v<float> / 2.0f, std::numbers::pi_v<float>, 0.0f });

    ParticleManager::GetInstance()->CreateParticleGroup("circle", "resources/circle.png");
    Transform t {};
    t.translate = { 0.0f, 0.0f, 0.0f };

    emitter_.Init("circle", t, 30, 0.1f);

    sphere_ = new SphereObject();
    sphere_->Initialize(DirectXCommon::GetInstance(), 16, 1.0f);

    // Transform
    sphere_->SetTranslate({ 0, 0, 0 }); // 消える？
    sphere_->SetScale({ 1.5f, 1.5f, 1.5f });

    // Material
    sphere_->SetColor({ 1, 1, 1, 1 });
}

void GamePlayScene::Update()
{
    // ImGuiのBegin/Endは絶対に呼ばない！
    emitter_.Update();
    ParticleManager::GetInstance()->Update();
    player2_->Update();
    sprite_->Update();
    sphere_->Update(camera_);
    camera_->Update();
    camera_->DebugUpdate();
}

void GamePlayScene::Draw3D()
{
    Object3dManager::GetInstance()->PreDraw();
    player2_->Draw();
    sphere_->Draw(DirectXCommon::GetInstance()->GetCommandList());
    ParticleManager::GetInstance()->PreDraw();
    ParticleManager::GetInstance()->Draw();
}

void GamePlayScene::Draw2D()
{
    SpriteManager::GetInstance()->PreDraw();

    // sprite_->Draw();
}

void GamePlayScene::DrawImGui()
{
#ifdef USE_IMGUI

#endif
}

void GamePlayScene::Finalize()
{
    ParticleManager::GetInstance()->Finalize();

    delete sprite_;
    sprite_ = nullptr;

    delete sphere_;
    sphere_ = nullptr;

    delete player2_;
    player2_ = nullptr;

    delete camera_;
    camera_ = nullptr;

    SoundManager::GetInstance()->SoundUnload(&bgm);
}

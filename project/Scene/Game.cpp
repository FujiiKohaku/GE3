#include "Game.h"
#include <numbers>

void Game::Initialize(WinApp* winApp, DirectXCommon* dxCommon, SrvManager* srvManager)
{
    winApp_ = winApp;
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
#pragma region Sprite関連
    spriteManager_ = new SpriteManager();
    spriteManager_->Initialize(dxCommon_);

    TextureManager::GetInstance()->LoadTexture("resources/uvChecker.png");

    sprite_ = new Sprite();
    sprite_->Initialize(spriteManager_, "resources/uvChecker.png");
    sprite_->SetPosition({ 100.0f, 100.0f });
#pragma endregion

#pragma region 3D関連
    object3dManager_ = new Object3dManager();
    object3dManager_->Initialize(dxCommon_);

    camera_ = new Camera();
    camera_->SetTranslate({ 0.0f, 0.0f, -1.0f });
    object3dManager_->SetDefaultCamera(camera_);
    

    modelCommon_.Initialize(dxCommon_);

    ModelManager::GetInstance()->initialize(dxCommon_);
    ModelManager::GetInstance()->LoadModel("plane.obj");
    ModelManager::GetInstance()->LoadModel("axis.obj");
    ModelManager::GetInstance()->LoadModel("titleTex.obj");
    ModelManager::GetInstance()->LoadModel("fence.obj");
    player2_.Initialize(object3dManager_);
    player2_.SetModel("fence.obj");
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
    particleManager_ = new ParticleManager();
    particleManager_->Initialize(dxCommon_, srvManager_,camera_);
 
#pragma endregion
}

void Game::Update()
{

    input_->Update();
    player2_.Update();
    camera_->Update();
    sprite_->Update();
    particleManager_->Update();
    camera_->DebugUpdate();
#ifdef USE_IMGUI

    // 現在の座標を取得
    Vector2 pos = sprite_->GetPosition();
    ImGui::SetNextWindowSize(ImVec2(500, 100), ImGuiCond_Always);
    // 実数4桁・小数1桁で表示
    // ImGui::SliderFloat2("Position", (float*)&pos, 0.0f, 500.0f, "%.1f");
    // 変更を反映
    sprite_->SetPosition(pos);
#endif

#ifdef USE_IMGUI
    ImGui::Begin("Player2 Debug");

    // 1. Transform（位置・回転・スケール）
    {
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
    }

    // 2. ライト調整
    {
        Object3d::DirectionalLight* light = player2_.GetLight();
        Object3d::Material* material = player2_.GetMaterial();

        ImGui::Separator();
        ImGui::Text("Directional Light");

        ImGui::ColorEdit3("Light Color", &light->color.x);
        ImGui::SliderFloat3("Light Direction", &light->direction.x, -1.0f, 1.0f);
        ImGui::SliderFloat("Intensity", &light->intensity, 0.0f, 5.0f);
        ImGui::SliderFloat("Alpha", &material->color.w, 0.0f, 1.0f);

        //  方向ベクトルの正規化
        light->direction = Normalize(light->direction);
    }
    ImGui::Separator();
    ImGui::Text("Blend Mode");

    // 現在のモードを取得
    int blendMode = object3dManager_->GetBlendMode();

    // 選択肢リスト
    const char* blendNames[] = {
        "None",
        "Normal ",
        "Add",
        "Subtract ",
        "Multiply",
        "Screen "
    };

    // ドロップダウン
    if (ImGui::Combo("Blend", &blendMode, blendNames, IM_ARRAYSIZE(blendNames))) {
        object3dManager_->SetBlendMode(static_cast<BlendMode>(blendMode));
    }

    ImGui::End();

#endif
}

void Game::Draw()
{
    // ① 3D描画ブロック
    {
        object3dManager_->PreDraw();

        player2_.Draw();

        particleManager_->PreDraw();
        particleManager_->Draw();
    }

    // ② 2D描画ブロック
    {
        spriteManager_->PreDraw();
        // sprite_->Draw();
    }
}

void Game::Finalize()
{
    delete object3dManager_;
    delete spriteManager_;
    delete sprite_;
    delete camera_;
    delete input_;
    delete particleManager_;
    ModelManager::GetInstance()->Finalize();
    soundManager_.Finalize(&bgm);
}

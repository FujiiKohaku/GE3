#include "ClearScene.h"
#include "Engine/Light/LightManager.h"
#include "Engine/input/Input.h"
#include "GamePlayScene.h"

#include "TitleScene.h"
void ClearScene::Initialize()
{
    camera_ = std::make_unique<Camera>();
    camera_->Initialize();
    camera_->SetTranslate({ 0.0f, 0.0f, -10.0f });
    camera_->SetRotate({ 0.0f, 0.0f, 0.0f });
    camera_->Update();
    Object3dManager::GetInstance()->SetDefaultCamera(camera_.get());
    ModelManager::GetInstance()->Load("axis.obj");
    LightManager::GetInstance()->SetDirectional({ 1, 1, 1, 1 }, { 0, -1, 0 }, 1.0f);
    TextureManager::GetInstance()->LoadTexture("resources/Textures/skybox.dds");
    Object3dManager::GetInstance()->SetEnvironmentTexture(TextureManager::GetInstance()->GetSrvHandleGPU("resources/Textures/skybox.dds"));
    titleObj_ = std::make_unique<Object3d>();
    titleObj_->Initialize(Object3dManager::GetInstance());
    titleObj_->SetModel(ModelManager::GetInstance()->FindModel("axis.obj"));
    titleObj_->SetTranslate({ 0.0f, 0.0f, 0.0f });
    titleObj_->SetRotate({ 0.0f, std::numbers::pi_v<float>, 0.0f });
    titleObj_->SetScale({ 1.0f, 1.0f, 1.0f });
    titleObj_->SetEnvironmentMapStrength(false);

    TextureManager::GetInstance()->LoadTexture("resources/Textures/Clear.png");
    // sprite
    titleSprite_ = std::make_unique<Sprite>();
    titleSprite_->Initialize(SpriteManager::GetInstance(), "resources/Textures/Clear.png");
    titleSprite_->SetSize({ 1280.0f, 720.0f });
}

void ClearScene::Update()
{
    if (Input::GetInstance()->IsKeyPressed(DIK_SPACE)) {
        SceneManager::GetInstance()->SetNextScene(std::make_unique<TitleScene>());
    }
    titleObj_->Update();
    titleSprite_->Update();
}

void ClearScene::Draw2D()
{
    SpriteManager::GetInstance()->PreDraw();
    titleSprite_->Draw();
    ;
}

void ClearScene::Draw3D()
{
    Object3dManager::GetInstance()->PreDraw();
    LightManager::GetInstance()->Bind(DirectXCommon::GetInstance()->GetCommandList());
    // titleObj_->Draw();
}

void ClearScene::DrawImGui()
{
}

void ClearScene::Finalize()
{
}
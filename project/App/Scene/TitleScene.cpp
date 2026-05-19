#include "TitleScene.h"
#include "Engine/Light/LightManager.h"
#include "Engine/input/Input.h"
#include "GamePlayScene.h"
void TitleScene::Initialize()
{
    camera_ = std::make_unique<Camera>();
    camera_->Initialize();
    camera_->SetTranslate({ 0.0f, 0.0f, -10.0f });
    camera_->SetRotate({ 0.0f, 0.0f, 0.0f });
    camera_->Update();

    Object3dManager::GetInstance()->SetDefaultCamera(camera_.get());
    ModelManager::GetInstance()->Load("axis.obj");

    LightManager::GetInstance()->SetDirectional({ 1, 1, 1, 1 }, { 0, -1, 0 }, 1.0f);
    TextureManager::GetInstance()->LoadTexture("resources/skyBox.dds");

    Object3dManager::GetInstance()->SetEnvironmentTexture(TextureManager::GetInstance()->GetSrvHandleGPU("resources/skyBox.dds"));
    titleObj_ = std::make_unique<Object3d>();
    titleObj_->Initialize(Object3dManager::GetInstance());
    titleObj_->SetModel(ModelManager::GetInstance()->FindModel("axis.obj"));
    titleObj_->SetTranslate({ 0.0f, 0.0f, 0.0f });
    titleObj_->SetRotate({ 0.0f, std::numbers::pi_v<float>, 0.0f });
    titleObj_->SetScale({ 1.0f, 1.0f, 1.0f });
    titleObj_->SetEnvironmentMapStrength(false);
    //  titleObj_->SetCamera(camera_.get());

    // sprite

   
     SceneManager::GetInstance()->SetPostEffectType(PostEffectType::GrayScale);

}
void TitleScene::Update()
{
    if (Input::GetInstance()->IsKeyPressed(DIK_SPACE)) {

        SceneManager::GetInstance()->SetNextScene(std::make_unique<GamePlayScene>());
    }

    titleObj_->Update();
    //titleSprite_->Update();

    if (Input::GetInstance()->IsKeyTrigger(DIK_1)) {
        SceneManager::GetInstance()->SetPostEffectType(PostEffectType::Copy);
    }

    if (Input::GetInstance()->IsKeyTrigger(DIK_2)) {
        SceneManager::GetInstance()->SetPostEffectType(PostEffectType::GrayScale);
    }
    if (Input::GetInstance()->IsKeyTrigger(DIK_3)) {
        SceneManager::GetInstance()->SetPostEffectType(PostEffectType::Vignette);
    }
    if (Input::GetInstance()->IsKeyTrigger(DIK_4)) {
        SceneManager::GetInstance()->SetPostEffectType(PostEffectType::smoothing);
    }
    if (Input::GetInstance()->IsKeyTrigger(DIK_5)) {
        SceneManager::GetInstance()->SetPostEffectType(PostEffectType::GaussianFilter);
    }
    if (Input::GetInstance()->IsKeyTrigger(DIK_6)) {
        SceneManager::GetInstance()->SetPostEffectType(PostEffectType::LuminanceBasedOutline);
    }
    if (Input::GetInstance()->IsKeyTrigger(DIK_7)) {
        SceneManager::GetInstance()->SetPostEffectType(PostEffectType::DepthOutline);
    }

    if (Input::GetInstance()->IsKeyTrigger(DIK_8)) {
        SceneManager::GetInstance()->SetPostEffectType(PostEffectType::RadialBlur);
    }
    if (Input::GetInstance()->IsKeyTrigger(DIK_9)) {
        SceneManager::GetInstance()->SetPostEffectType(PostEffectType::Dissolve);
    }
}

void TitleScene::Draw2D()
{
    SpriteManager::GetInstance()->PreDraw();
    

}

void TitleScene::Draw3D()
{
    Object3dManager::GetInstance()->PreDraw();
    LightManager::GetInstance()->Bind(DirectXCommon::GetInstance()->GetCommandList());
    titleObj_->Draw();
}

void TitleScene::DrawImGui()
{
}

void TitleScene::Finalize()
{
}
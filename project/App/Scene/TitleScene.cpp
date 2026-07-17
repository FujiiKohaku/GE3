#include "TitleScene.h"
#include "Engine/2D/SpriteManager.h"
#include "Engine/3D/ModelManager.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/DirectXCommon/DirectXCommon.h"
#include "Engine/Light/LightManager.h"
#include "Engine/input/Input.h"
#include "LoadingScene.h"
#include "GamePlayScene.h"
#include "TestScene1.h"
#include "externals/imgui/imgui.h"

#include <numbers>
void TitleScene::Initialize()
{
    camera_ = std::make_unique<Camera>();
    camera_->Initialize();
    camera_->SetTranslate({ 0.0f, 0.0f, -10.0f });
    camera_->SetRotate({ 0.0f, 0.0f, 0.0f });
    camera_->Update();

    Object3dManager::GetInstance()->SetDefaultCamera(camera_.get());
    // Model Path
    const char* axisModelPath = "Debug/Axis/axis.obj";
    ModelManager::GetInstance()->Load(axisModelPath);

    LightManager::GetInstance()->SetDirectional({ 1, 1, 1, 1 }, { 0, -1, 0 }, 1.0f);
    TextureManager::GetInstance()->LoadTexture("resources/Textures/skybox.dds");

    Object3dManager::GetInstance()->SetEnvironmentTexture(TextureManager::GetInstance()->GetSrvHandleGPU("resources/Textures/skybox.dds"));
    titleObj_ = std::make_unique<Object3d>();
    titleObj_->Initialize(Object3dManager::GetInstance());
    titleObj_->SetModel(ModelManager::GetInstance()->FindModel(axisModelPath));
    titleObj_->SetTranslate({ 0.0f, 0.0f, 0.0f });
    titleObj_->SetRotate({ 0.0f, std::numbers::pi_v<float>, 0.0f });
    titleObj_->SetScale({ 1.0f, 1.0f, 1.0f });
    titleObj_->SetEnableEnvironmentMap(false);
    titleObj_->SetEnvironmentMapStrength(0.0f);
    //  titleObj_->SetCamera(camera_.get());

    TextureManager::GetInstance()->LoadTexture("resources/Textures/Credit.png");
    TextureManager::GetInstance()->LoadTexture("resources/Textures/exit.png");
    // sprite
    titleSprite_ = std::make_unique<Sprite>();
    titleSprite_->Initialize(SpriteManager::GetInstance(), "resources/Textures/Credit.png");

    creditSprite_ = std::make_unique<Sprite>();
    creditSprite_->Initialize(SpriteManager::GetInstance(), "resources/Textures/exit.png");
    creditSprite_->SetPosition({ 210.0f, 0.0f });
    SceneManager::GetInstance()->SetPostEffectType(PostEffectType::GrayScale);
}
void TitleScene::Update()
{
    if (Input::GetInstance()->IsKeyPressed(DIK_SPACE)) {
        SceneManager::GetInstance()->SetNextSceneWithLoading<LoadingScene, GamePlayScene>();
    }

    if (Input::GetInstance()->IsKeyTrigger(DIK_T)) {
        SceneManager::GetInstance()->SetNextSceneWithLoading<LoadingScene, TestScene1>();
    }

    if (Input::GetInstance()->IsKeyTrigger(DIK_L)) {
        isRandomPostEffect_ = !isRandomPostEffect_;
        if (isRandomPostEffect_) {
            SceneManager::GetInstance()->SetPostEffectType(PostEffectType::Random);
        } else {
            SceneManager::GetInstance()->SetPostEffectType(PostEffectType::GrayScale);
        }
    }

    titleObj_->Update();
    titleSprite_->Update();
    creditSprite_->Update();

}

void TitleScene::Draw2D()
{
    SpriteManager::GetInstance()->PreDraw();

    titleSprite_->Draw();
    creditSprite_->Draw();
}

void TitleScene::Draw3D()
{
    Object3dManager::GetInstance()->PreDraw();
    LightManager::GetInstance()->Bind(DirectXCommon::GetInstance()->GetCommandList());
    titleObj_->Draw();
}

void TitleScene::DrawParticle()
{
}

void TitleScene::DrawImGui()
{
    ImGui::Begin("Title Scene");
    ImGui::Text("Press SPACE to start GamePlayScene");
    ImGui::Text("Press T to start TestScene1");
    ImGui::End();
}

void TitleScene::Finalize()
{
}

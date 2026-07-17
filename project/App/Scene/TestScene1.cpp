#include "TestScene1.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/Light/LightManager.h"
#include "Engine/input/Input.h"
#include "TitleScene.h"
#include "externals/imgui/imgui.h"
#include "Engine/3D/ModelManager.h"
#include "Engine/DirectXCommon/DirectXCommon.h"
#include <numbers>

void TestScene1::Initialize()
{
    camera_ = std::make_unique<Camera>();
    camera_->Initialize();
    camera_->SetTranslate({ 0.0f, 0.0f, -10.0f });
    camera_->SetRotate({ 0.0f, 0.0f, 0.0f });
    camera_->Update();
    Object3dManager::GetInstance()->SetDefaultCamera(camera_.get());

    LightManager::GetInstance()->SetDirectional({ 1.0f, 1.0f, 1.0f, 1.0f }, { 0.0f, -1.0f, 0.0f }, 1.0f);

    // floorの初期化
    Model* floorModel = ModelManager::GetInstance()->CreatePlane("resources/Textures/floor_dirt_gemini.jpg", 10.0f, 10.0f);
    floorObj_ = std::make_unique<Object3d>();
    floorObj_->Initialize(Object3dManager::GetInstance());
    floorObj_->SetModel(floorModel);
    floorObj_->SetTranslate({ 0.0f, -5.0f, 0.0f });
    floorObj_->SetRotate({ std::numbers::pi_v<float> / 2.0f, 0.0f, 0.0f });
    floorObj_->SetScale({ 100.0f, 100.0f, 1.0f });
}

void TestScene1::Update()
{
    // タイトルシーンに戻る
    if (Input::GetInstance()->IsKeyTrigger(DIK_SPACE)) {
        SceneManager::GetInstance()->SetNextScene(std::make_unique<TitleScene>());
    }

    if (floorObj_) {
        floorObj_->Update();
    }
}

void TestScene1::Draw2D()
{
}

void TestScene1::Draw3D()
{
    Object3dManager::GetInstance()->PreDraw();
    LightManager::GetInstance()->Bind(DirectXCommon::GetInstance()->GetCommandList());
    if (floorObj_) {
        floorObj_->Draw();
    }
}

void TestScene1::DrawParticle()
{
}

void TestScene1::DrawImGui()
{
    ImGui::Begin("Test Scene 1");
    ImGui::Text("This is Test Scene 1.");
    ImGui::Text("Press SPACE to return to Title Scene.");
    ImGui::End();
}

void TestScene1::Finalize()
{
}

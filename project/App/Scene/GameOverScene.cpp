#include "GameOverScene.h"
#include "Engine/Light/LightManager.h"
#include "Engine/input/Input.h"
#include "Engine/2D/Text/TextRenderer.h"
#include "GamePlayScene.h"

#include "TitleScene.h"
void GameOverScene::Initialize()
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

    TextureManager::GetInstance()->LoadTexture("resources/Textures/gameover.png");
    // sprite
    titleSprite_ = std::make_unique<Sprite>();
    titleSprite_->Initialize(SpriteManager::GetInstance(), "resources/Textures/gameover.png");
    titleSprite_->SetSize({ 1280.0f, 720.0f });

    retryGuideText_ = std::make_unique<Text>();
    retryGuideText_->Initialize(
        "resources/Fonts/NotoSansJP/NotoSansJP-Variable.ttf");
    retryGuideText_->SetText("リトライ [R]     タイトルに戻る [T]");
    retryGuideText_->SetPosition({ 640.0f, 620.0f });
    retryGuideText_->SetAnchorPoint({ 0.5f, 0.5f });
    retryGuideText_->SetFontSize(28.0f);
    retryGuideText_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
    retryGuideText_->SetOutlineWidth(1.0f);
}

void GameOverScene::Update()
{
    if (Input::GetInstance()->IsKeyTrigger(DIK_R)) {
        SceneManager::GetInstance()->SetNextScene(
            std::make_unique<GamePlayScene>(stageId_));
    } else if (Input::GetInstance()->IsKeyTrigger(DIK_T)) {
        SceneManager::GetInstance()->SetNextScene(std::make_unique<TitleScene>());
    }
    titleObj_->Update();
    titleSprite_->Update();
    retryGuideText_->Update();
}

void GameOverScene::Draw2D()
{
    SpriteManager::GetInstance()->PreDraw();
    titleSprite_->Draw();
    TextRenderer::GetInstance()->PreDraw();
    retryGuideText_->Draw();
}

void GameOverScene::Draw3D()
{
    Object3dManager::GetInstance()->PreDraw();
    LightManager::GetInstance()->Bind(DirectXCommon::GetInstance()->GetCommandList());
    // titleObj_->Draw();
}

void GameOverScene::DrawParticle()
{
}

void GameOverScene::DrawImGui()
{
}

void GameOverScene::Finalize()
{
}

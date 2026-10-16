// クリア時の表示と入力を扱い、次の画面へ移るまでの結果画面を構成する。
#include "App/Scene/Result/ClearScene.h"
#include "Engine/Light/LightManager.h"
#include "Engine/input/Input.h"
#include "Engine/2D/Text/TextRenderer.h"
#include "App/Scene/Gameplay/GamePlayScene.h"

#include "App/Scene/Title/TitleScene.h"
#include "Engine/audio/SoundManager.h"
// クリア表示用のモデル・文字と画面配置を準備する。
void ClearScene::Initialize()
{
    returnGuideText_ = std::make_unique<Text>();
    returnGuideText_->Initialize("resources/Fonts/NotoSansJP/NotoSansJP-Variable.ttf");
    returnGuideText_->SetPosition({ 640.0f, 650.0f });
    returnGuideText_->SetAnchorPoint({ 0.5f, 0.5f });
    returnGuideText_->SetFontSize(26.0f);
    returnGuideText_->SetOutlineWidth(1.0f);
    SoundManager::GetInstance()->RegisterSounds({
        { "StageClear", "resources/Audio/GameSfx/stage_clear.wav", 0.34f, 0.50f },
    });
    SoundManager::GetInstance()->Play("StageClear");
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

    TextureManager::GetInstance()->LoadTexture("resources/Textures/Clear.png");
    // sprite
    titleSprite_ = std::make_unique<Sprite>();
    titleSprite_->Initialize(SpriteManager::GetInstance(), "resources/Textures/Clear.png");
    titleSprite_->SetSize({ 1280.0f, 720.0f });
}

// クリア画面の入力を受け付け、選ばれた画面への遷移を予約する。
void ClearScene::Update()
{
    if (Input::GetInstance()->IsKeyTrigger(DIK_SPACE) ||
        Input::GetInstance()->IsGamepadButtonTrigger(XINPUT_GAMEPAD_A) ||
        Input::GetInstance()->IsGamepadButtonTrigger(XINPUT_GAMEPAD_B)) {
        SceneManager::GetInstance()->SetNextScene(std::make_unique<TitleScene>(true));
    }
    titleObj_->Update();
    titleSprite_->Update();
    if (Input::GetInstance()->IsUsingGamepad()) {
        returnGuideText_->SetText("ステージセレクトに戻る [A / B]");
    } else {
        returnGuideText_->SetText("ステージセレクトに戻る [SPACE]");
    }
    returnGuideText_->Update();
}

// 画面に重ねるスプライトや文字を描画する。
void ClearScene::Draw2D()
{
    SpriteManager::GetInstance()->PreDraw();
    titleSprite_->Draw();
    TextRenderer::GetInstance()->PreDraw();
    returnGuideText_->Draw();
    ;
}

// 現在表示するモデルや追加パーツを描画する。
void ClearScene::Draw3D()
{
    Object3dManager::GetInstance()->PreDraw();
    // titleObj_->Draw();
}

// このシーンではこの描画経路を使用しないため、処理を行わない。
void ClearScene::DrawParticle()
{
}

// このシーンではこの描画経路を使用しないため、処理を行わない。
void ClearScene::DrawImGui()
{
}

void ClearScene::Finalize()
{
}

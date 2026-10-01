#include "TextTestScene.h"

#include "Engine/2D/SpriteManager.h"
#include "Engine/2D/Text/TextRenderer.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/ImGuiManager/ImGuiManager.h"
#include "Engine/PostEffect/PostEffectType.h"
#include "Engine/input/Input.h"
#include "SceneManager.h"
#include "TitleScene.h"
#include <string>

namespace {
constexpr const char* kWhiteTexture = "resources/Textures/white.png";
constexpr const char* kDefaultFont =
    "resources/Fonts/NotoSansJP/NotoSansJP-Variable.ttf";
constexpr Vector4 kBackgroundColor = { 0.012f, 0.018f, 0.038f, 1.0f };
}

void TextTestScene::Initialize()
{
    Object3dManager::GetInstance()->SetDefaultCamera(nullptr);
    SceneManager::GetInstance()->SetPostEffectType(PostEffectType::Copy);

    backgroundSprite_ = std::make_unique<Sprite>();
    backgroundSprite_->Initialize(SpriteManager::GetInstance(), kWhiteTexture);
    backgroundSprite_->SetSize({ 1280.0f, 720.0f });
    backgroundSprite_->SetColor(kBackgroundColor);

    titleText_ = std::make_unique<Text>();
    titleText_->Initialize(kDefaultFont);
    titleText_->SetText("TEXT RENDERING LAB / 文字描画テスト");
    titleText_->SetPosition({ 640.0f, 38.0f });
    titleText_->SetAnchorPoint({ 0.5f, 0.0f });
    titleText_->SetFontSize(48.0f);
    titleText_->SetColor({ 0.35f, 0.85f, 1.0f, 1.0f });
    titleText_->SetOutlineColor({ 0.0f, 0.12f, 0.22f, 1.0f });
    titleText_->SetOutlineWidth(2.0f);
    titleText_->SetShadowOffset({ 3.0f, 3.0f });

    japaneseText_ = std::make_unique<Text>();
    japaneseText_->Initialize(kDefaultFont);
    japaneseText_->SetText("日本語・English・1234567890を同じフォントで表示できます");
    japaneseText_->SetPosition({ 640.0f, 112.0f });
    japaneseText_->SetAnchorPoint({ 0.5f, 0.0f });
    japaneseText_->SetFontSize(29.0f);
    japaneseText_->SetColor({ 1.0f, 0.94f, 0.66f, 1.0f });

    wrappingText_ = std::make_unique<Text>();
    wrappingText_->Initialize(kDefaultFont);
    wrappingText_->SetText(
        "長い文章は指定された横幅で自動的に折り返されます。ゲーム内の会話、説明文、"
        "チュートリアルなどにも利用できる文字描画システムです。");
    wrappingText_->SetPosition({ 80.0f, 170.0f });
    wrappingText_->SetFontSize(25.0f);
    wrappingText_->SetMaxWidth(1120.0f);
    wrappingText_->SetLineSpacing(8.0f);
    wrappingText_->SetColor({ 0.88f, 0.92f, 1.0f, 1.0f });

    leftText_ = std::make_unique<Text>();
    leftText_->Initialize(kDefaultFont);
    leftText_->SetText("左揃え\nLEFT ALIGN");
    leftText_->SetPosition({ 80.0f, 330.0f });
    leftText_->SetFontSize(28.0f);
    leftText_->SetColor({ 1.0f, 0.45f, 0.48f, 1.0f });

    centerText_ = std::make_unique<Text>();
    centerText_->Initialize(kDefaultFont);
    centerText_->SetText("中央揃え\nCENTER ALIGN");
    centerText_->SetPosition({ 640.0f, 330.0f });
    centerText_->SetAnchorPoint({ 0.5f, 0.0f });
    centerText_->SetFontSize(28.0f);
    centerText_->SetMaxWidth(300.0f);
    centerText_->SetHorizontalAlignment(TextHorizontalAlignment::Center);
    centerText_->SetColor({ 0.44f, 1.0f, 0.65f, 1.0f });

    rightText_ = std::make_unique<Text>();
    rightText_->Initialize(kDefaultFont);
    rightText_->SetText("右揃え\nRIGHT ALIGN");
    rightText_->SetPosition({ 1200.0f, 330.0f });
    rightText_->SetAnchorPoint({ 1.0f, 0.0f });
    rightText_->SetFontSize(28.0f);
    rightText_->SetMaxWidth(300.0f);
    rightText_->SetHorizontalAlignment(TextHorizontalAlignment::Right);
    rightText_->SetColor({ 0.78f, 0.56f, 1.0f, 1.0f });

    smallText_ = std::make_unique<Text>();
    smallText_->Initialize(kDefaultFont);
    smallText_->SetText("Small 18px / 小さい文字");
    smallText_->SetPosition({ 80.0f, 460.0f });
    smallText_->SetFontSize(18.0f);

    mediumText_ = std::make_unique<Text>();
    mediumText_->Initialize(kDefaultFont);
    mediumText_->SetText("Medium 32px / 標準サイズ");
    mediumText_->SetPosition({ 80.0f, 500.0f });
    mediumText_->SetFontSize(32.0f);
    mediumText_->SetOutlineWidth(1.0f);

    largeText_ = std::make_unique<Text>();
    largeText_->Initialize(kDefaultFont);
    largeText_->SetText("Large 56px");
    largeText_->SetPosition({ 80.0f, 550.0f });
    largeText_->SetFontSize(56.0f);
    largeText_->SetColor({ 1.0f, 0.68f, 0.25f, 1.0f });
    largeText_->SetOutlineColor({ 0.25f, 0.06f, 0.0f, 1.0f });
    largeText_->SetOutlineWidth(2.0f);
    largeText_->SetShadowOffset({ 4.0f, 4.0f });

    dynamicText_ = std::make_unique<Text>();
    dynamicText_->Initialize(kDefaultFont);
    dynamicText_->SetPosition({ 760.0f, 500.0f });
    dynamicText_->SetFontSize(26.0f);
    dynamicText_->SetColor({ 0.35f, 1.0f, 0.92f, 1.0f });

    instructionText_ = std::make_unique<Text>();
    instructionText_->Initialize(kDefaultFont);
    instructionText_->SetText("BACKSPACE : タイトルへ戻る");
    instructionText_->SetPosition({ 1200.0f, 690.0f });
    instructionText_->SetAnchorPoint({ 1.0f, 1.0f });
    instructionText_->SetFontSize(20.0f);
    instructionText_->SetColor({ 0.65f, 0.72f, 0.86f, 1.0f });
}

void TextTestScene::Finalize()
{
}

void TextTestScene::Update()
{
    if (Input::GetInstance()->IsKeyTrigger(DIK_BACKSPACE)) {
        SceneManager::GetInstance()->SetNextScene(std::make_unique<TitleScene>());
        return;
    }

    frameCount_++;
    dynamicText_->SetText(
        "Dynamic text / フレーム: " + std::to_string(frameCount_));

    backgroundSprite_->Update();
    titleText_->Update();
    japaneseText_->Update();
    wrappingText_->Update();
    leftText_->Update();
    centerText_->Update();
    rightText_->Update();
    smallText_->Update();
    mediumText_->Update();
    largeText_->Update();
    dynamicText_->Update();
    instructionText_->Update();
}

void TextTestScene::Draw2D()
{
    SpriteManager::GetInstance()->PreDraw();
    backgroundSprite_->Draw();

    TextRenderer::GetInstance()->PreDraw();
    titleText_->Draw();
    japaneseText_->Draw();
    wrappingText_->Draw();
    leftText_->Draw();
    centerText_->Draw();
    rightText_->Draw();
    smallText_->Draw();
    mediumText_->Draw();
    largeText_->Draw();
    dynamicText_->Draw();
    instructionText_->Draw();
}

void TextTestScene::Draw3D()
{
}

void TextTestScene::DrawParticle()
{
}

void TextTestScene::DrawImGui()
{
#ifdef USE_IMGUI
    ImGui::Begin("Text Test Scene");
    ImGui::Text("Game text is rendered without ImGui.");
    ImGui::Text("BACKSPACE: Return to Title");
    ImGui::End();
#endif
}

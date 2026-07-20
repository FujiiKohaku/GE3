#include "TitleScene.h"

#include "Engine/2D/SpriteManager.h"
#include "Engine/2D/Text/TextRenderer.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/ImGuiManager/ImGuiManager.h"
#include "Engine/WinApp/WinApp.h"
#include "Engine/input/Input.h"
#include "GamePlayScene.h"
#include "LoadingScene.h"
#include "SpriteTestScene.h"
#include "TestScene1.h"
#include "TextTestScene.h"

namespace {
constexpr const char* kWhiteTexture = "resources/Textures/white.png";
constexpr const char* kDefaultFont =
    "resources/Fonts/NotoSansJP/NotoSansJP-Variable.ttf";
constexpr const char* kWaveVertexShader =
    "resources/Shaders/Sprite/Wave/Render.VS.hlsl";
constexpr const char* kWavePixelShader =
    "resources/Shaders/Sprite/Wave/Render.PS.hlsl";

constexpr float kButtonTop = 265.0f;
constexpr float kButtonWidth = 300.0f;
constexpr float kButtonHeight = 116.0f;
constexpr float kGamePlayButtonLeft = 100.0f;
constexpr float kTestButtonLeft = 490.0f;
constexpr float kSpriteTestButtonLeft = 880.0f;
constexpr float kTextTestButtonLeft = 1080.0f;
constexpr float kTextTestButtonTop = 35.0f;
constexpr float kTextTestButtonWidth = 170.0f;
constexpr float kTextTestButtonHeight = 58.0f;
constexpr float kButtonAnimationSpeed = 0.25f;
constexpr float kHoverScale = 1.04f;
constexpr float kPressedScale = 0.94f;
constexpr float kPressedOffsetY = 7.0f;

constexpr Vector4 kBackgroundColor = { 0.02f, 0.025f, 0.05f, 1.0f };
constexpr Vector4 kButtonColor = { 0.09f, 0.11f, 0.18f, 1.0f };
constexpr Vector4 kButtonHoverColor = { 0.18f, 0.42f, 0.68f, 1.0f };
constexpr Vector4 kButtonPressedColor = { 0.07f, 0.24f, 0.42f, 1.0f };
constexpr Vector4 kButtonTextColor = { 0.78f, 0.92f, 1.0f, 1.0f };
constexpr Vector4 kButtonPressedTextColor = { 0.55f, 0.78f, 0.92f, 1.0f };
constexpr Vector4 kHeaderColor = { 0.20f, 0.72f, 1.0f, 1.0f };
}

void TitleScene::Initialize()
{
    SetMouseCursorVisible(true);
    ClipCursor(nullptr);
    Object3dManager::GetInstance()->SetDefaultCamera(nullptr);

    backgroundSprite_ = std::make_unique<Sprite>();
    backgroundSprite_->Initialize(SpriteManager::GetInstance(), kWhiteTexture);
    backgroundSprite_->SetSize({ 1280.0f, 720.0f });
    backgroundSprite_->SetColor(kBackgroundColor);

    flagSprite_ = std::make_unique<Sprite>();
    flagSprite_->Initialize(SpriteManager::GetInstance(), kWhiteTexture);
    flagSprite_->SetPosition({ 60.0f, 70.0f });
    flagSprite_->SetSize({ 140.0f, 70.0f });
    flagSprite_->SetAnchorPoint({ 0.0f, 0.5f });
    flagSprite_->SetColor(kHeaderColor);
    flagSprite_->SetShaderPaths(kWaveVertexShader, kWavePixelShader);
    flagSprite_->SetGridMesh(24, 8);
    flagSprite_->SetEffectAmplitude(10.0f);
    flagSprite_->SetEffectFrequency(1.5f);
    flagSprite_->SetEffectSpeed(3.0f);
    flagSprite_->SetEffectDirection({ 0.0f, 1.0f });

    headerLineSprite_ = std::make_unique<Sprite>();
    headerLineSprite_->Initialize(SpriteManager::GetInstance(), kWhiteTexture);
    headerLineSprite_->SetPosition({ 220.0f, 105.0f });
    headerLineSprite_->SetSize({ 840.0f, 8.0f });
    headerLineSprite_->SetColor(kHeaderColor);

    gamePlayButtonSprite_ = std::make_unique<Sprite>();
    gamePlayButtonSprite_->Initialize(SpriteManager::GetInstance(), kWhiteTexture);
    gamePlayButtonSprite_->SetPosition({ kGamePlayButtonLeft, kButtonTop });
    gamePlayButtonSprite_->SetSize({ kButtonWidth, kButtonHeight });

    gamePlayButtonText_ = std::make_unique<Text>();
    gamePlayButtonText_->Initialize(kDefaultFont);
    gamePlayButtonText_->SetText("GamePlayScene");
    gamePlayButtonText_->SetPosition({
        kGamePlayButtonLeft + kButtonWidth * 0.5f,
        kButtonTop + kButtonHeight * 0.5f
    });
    gamePlayButtonText_->SetAnchorPoint({ 0.5f, 0.5f });
    gamePlayButtonText_->SetFontSize(32.0f);
    gamePlayButtonText_->SetColor(kButtonTextColor);
    gamePlayButtonText_->SetOutlineWidth(1.0f);

    testButtonSprite_ = std::make_unique<Sprite>();
    testButtonSprite_->Initialize(SpriteManager::GetInstance(), kWhiteTexture);
    testButtonSprite_->SetPosition({ kTestButtonLeft, kButtonTop });
    testButtonSprite_->SetSize({ kButtonWidth, kButtonHeight });

    testButtonText_ = std::make_unique<Text>();
    testButtonText_->Initialize(kDefaultFont);
    testButtonText_->SetText("TestScene1");
    testButtonText_->SetPosition({
        kTestButtonLeft + kButtonWidth * 0.5f,
        kButtonTop + kButtonHeight * 0.5f
    });
    testButtonText_->SetAnchorPoint({ 0.5f, 0.5f });
    testButtonText_->SetFontSize(32.0f);
    testButtonText_->SetColor(kButtonTextColor);
    testButtonText_->SetOutlineWidth(1.0f);

    spriteTestButtonSprite_ = std::make_unique<Sprite>();
    spriteTestButtonSprite_->Initialize(SpriteManager::GetInstance(), kWhiteTexture);
    spriteTestButtonSprite_->SetPosition({ kSpriteTestButtonLeft, kButtonTop });
    spriteTestButtonSprite_->SetSize({ kButtonWidth, kButtonHeight });

    spriteTestButtonText_ = std::make_unique<Text>();
    spriteTestButtonText_->Initialize(kDefaultFont);
    spriteTestButtonText_->SetText("SpriteTestScene");
    spriteTestButtonText_->SetPosition({
        kSpriteTestButtonLeft + kButtonWidth * 0.5f,
        kButtonTop + kButtonHeight * 0.5f
    });
    spriteTestButtonText_->SetAnchorPoint({ 0.5f, 0.5f });
    spriteTestButtonText_->SetFontSize(30.0f);
    spriteTestButtonText_->SetColor(kButtonTextColor);
    spriteTestButtonText_->SetOutlineWidth(1.0f);

    textTestButtonSprite_ = std::make_unique<Sprite>();
    textTestButtonSprite_->Initialize(SpriteManager::GetInstance(), kWhiteTexture);
    textTestButtonSprite_->SetPosition({ kTextTestButtonLeft, kTextTestButtonTop });
    textTestButtonSprite_->SetSize({ kTextTestButtonWidth, kTextTestButtonHeight });

    textTestButtonText_ = std::make_unique<Text>();
    textTestButtonText_->Initialize(kDefaultFont);
    textTestButtonText_->SetText("TEXT TEST");
    textTestButtonText_->SetPosition({
        kTextTestButtonLeft + kTextTestButtonWidth * 0.5f,
        kTextTestButtonTop + kTextTestButtonHeight * 0.5f
    });
    textTestButtonText_->SetAnchorPoint({ 0.5f, 0.5f });
    textTestButtonText_->SetFontSize(22.0f);
    textTestButtonText_->SetColor(kHeaderColor);

    SceneManager::GetInstance()->SetPostEffectType(PostEffectType::Copy);
}

void TitleScene::Update()
{
    const bool isGamePlayHovered = IsMouseOver(
        kGamePlayButtonLeft,
        kButtonTop,
        kButtonWidth,
        kButtonHeight);
    const bool isTestHovered = IsMouseOver(
        kTestButtonLeft,
        kButtonTop,
        kButtonWidth,
        kButtonHeight);
    const bool isSpriteTestHovered = IsMouseOver(
        kSpriteTestButtonLeft,
        kButtonTop,
        kButtonWidth,
        kButtonHeight);
    const bool isTextTestHovered = IsMouseOver(
        kTextTestButtonLeft,
        kTextTestButtonTop,
        kTextTestButtonWidth,
        kTextTestButtonHeight);

    Input* input = Input::GetInstance();
    const bool isMousePressed = input->IsMousePressed(0);
    if (input->IsMouseTrigger(0)) {
        if (isGamePlayHovered) {
            pressedButtonIndex_ = 0;
        } else if (isTestHovered) {
            pressedButtonIndex_ = 1;
        } else if (isSpriteTestHovered) {
            pressedButtonIndex_ = 2;
        } else if (isTextTestHovered) {
            pressedButtonIndex_ = 3;
        }
    }

    bool isGamePlayPressed = false;
    bool isTestPressed = false;
    bool isSpriteTestPressed = false;
    bool isTextTestPressed = false;
    if (isMousePressed && pressedButtonIndex_ == 0) {
        isGamePlayPressed = true;
    }
    if (isMousePressed && pressedButtonIndex_ == 1) {
        isTestPressed = true;
    }
    if (isMousePressed && pressedButtonIndex_ == 2) {
        isSpriteTestPressed = true;
    }
    if (isMousePressed && pressedButtonIndex_ == 3) {
        isTextTestPressed = true;
    }

    UpdateButtonVisual(
        gamePlayButtonSprite_.get(),
        gamePlayButtonText_.get(),
        kGamePlayButtonLeft,
        kButtonTop,
        kButtonWidth,
        kButtonHeight,
        isGamePlayHovered,
        isGamePlayPressed,
        0);
    UpdateButtonVisual(
        testButtonSprite_.get(),
        testButtonText_.get(),
        kTestButtonLeft,
        kButtonTop,
        kButtonWidth,
        kButtonHeight,
        isTestHovered,
        isTestPressed,
        1);
    UpdateButtonVisual(
        spriteTestButtonSprite_.get(),
        spriteTestButtonText_.get(),
        kSpriteTestButtonLeft,
        kButtonTop,
        kButtonWidth,
        kButtonHeight,
        isSpriteTestHovered,
        isSpriteTestPressed,
        2);
    UpdateButtonVisual(
        textTestButtonSprite_.get(),
        textTestButtonText_.get(),
        kTextTestButtonLeft,
        kTextTestButtonTop,
        kTextTestButtonWidth,
        kTextTestButtonHeight,
        isTextTestHovered,
        isTextTestPressed,
        3);

    const bool wasReleased = !isMousePressed && wasMousePressed_;
    if (wasReleased) {
        const int releasedButtonIndex = pressedButtonIndex_;
        pressedButtonIndex_ = -1;
        if (releasedButtonIndex == 0 && isGamePlayHovered) {
            SceneManager::GetInstance()->SetNextSceneWithLoading<LoadingScene, GamePlayScene>();
        } else if (releasedButtonIndex == 1 && isTestHovered) {
            SceneManager::GetInstance()->SetNextSceneWithLoading<LoadingScene, TestScene1>();
        } else if (releasedButtonIndex == 2 && isSpriteTestHovered) {
            SceneManager::GetInstance()->SetNextScene(std::make_unique<SpriteTestScene>());
        } else if (releasedButtonIndex == 3 && isTextTestHovered) {
            SceneManager::GetInstance()->SetNextScene(std::make_unique<TextTestScene>());
        }
    }
    wasMousePressed_ = isMousePressed;

    backgroundSprite_->Update();
    flagSprite_->Update();
    headerLineSprite_->Update();
    gamePlayButtonSprite_->Update();
    gamePlayButtonText_->Update();
    testButtonSprite_->Update();
    testButtonText_->Update();
    spriteTestButtonSprite_->Update();
    spriteTestButtonText_->Update();
    textTestButtonSprite_->Update();
    textTestButtonText_->Update();
}

void TitleScene::Draw2D()
{
    SpriteManager::GetInstance()->PreDraw();
    backgroundSprite_->Draw();
    flagSprite_->Draw();
    headerLineSprite_->Draw();
    gamePlayButtonSprite_->Draw();
    testButtonSprite_->Draw();
    spriteTestButtonSprite_->Draw();
    textTestButtonSprite_->Draw();

    TextRenderer::GetInstance()->PreDraw();
    gamePlayButtonText_->Draw();
    testButtonText_->Draw();
    spriteTestButtonText_->Draw();
    textTestButtonText_->Draw();
}

void TitleScene::Draw3D()
{
}

void TitleScene::DrawParticle()
{
}

void TitleScene::DrawImGui()
{
#ifdef USE_IMGUI
    ImGui::Begin("Title Scene");
    ImGui::Text("GamePlayScene button");
    ImGui::Text("TestScene1 button");
    ImGui::Text("SpriteTestScene button");
    ImGui::Text("TextTestScene button");
    ImGui::End();
#endif
}

void TitleScene::Finalize()
{
    SetMouseCursorVisible(false);

    RECT clientRect {};
    GetClientRect(WinApp::GetInstance()->GetHwnd(), &clientRect);
    POINT leftTop { clientRect.left, clientRect.top };
    POINT rightBottom { clientRect.right, clientRect.bottom };
    ClientToScreen(WinApp::GetInstance()->GetHwnd(), &leftTop);
    ClientToScreen(WinApp::GetInstance()->GetHwnd(), &rightBottom);
    RECT screenRect { leftTop.x, leftTop.y, rightBottom.x, rightBottom.y };
    ClipCursor(&screenRect);
}

bool TitleScene::IsMouseOver(float left, float top, float width, float height) const
{
    POINT mousePosition {};
    if (!GetCursorPos(&mousePosition)) {
        return false;
    }
    if (!ScreenToClient(WinApp::GetInstance()->GetHwnd(), &mousePosition)) {
        return false;
    }

    return mousePosition.x >= left &&
        mousePosition.x < left + width &&
        mousePosition.y >= top &&
        mousePosition.y < top + height;
}

void TitleScene::SetMouseCursorVisible(bool visible) const
{
    if (visible) {
        while (ShowCursor(TRUE) < 0) {
        }
    } else {
        while (ShowCursor(FALSE) >= 0) {
        }
    }
}

void TitleScene::UpdateButtonVisual(
    Sprite* buttonSprite,
    Text* buttonText,
    float baseLeft,
    float baseTop,
    float baseWidth,
    float baseHeight,
    bool isHovered,
    bool isPressed,
    std::size_t animationIndex)
{
    float targetScale = 1.0f;
    float targetOffsetY = 0.0f;
    Vector4 targetButtonColor = kButtonColor;
    Vector4 targetTextColor = kButtonTextColor;

    if (isHovered) {
        targetScale = kHoverScale;
        targetButtonColor = kButtonHoverColor;
    }
    if (isPressed) {
        targetScale = kPressedScale;
        targetOffsetY = kPressedOffsetY;
        targetButtonColor = kButtonPressedColor;
        targetTextColor = kButtonPressedTextColor;
    }

    buttonScales_[animationIndex] +=
        (targetScale - buttonScales_[animationIndex]) * kButtonAnimationSpeed;
    buttonOffsetsY_[animationIndex] +=
        (targetOffsetY - buttonOffsetsY_[animationIndex]) * kButtonAnimationSpeed;

    const float animatedWidth = baseWidth * buttonScales_[animationIndex];
    const float animatedHeight = baseHeight * buttonScales_[animationIndex];
    const float animatedLeft =
        baseLeft + (baseWidth - animatedWidth) * 0.5f;
    const float animatedTop =
        baseTop + (baseHeight - animatedHeight) * 0.5f +
        buttonOffsetsY_[animationIndex];

    buttonSprite->SetPosition({ animatedLeft, animatedTop });
    buttonSprite->SetSize({ animatedWidth, animatedHeight });
    buttonSprite->SetColor(targetButtonColor);
    buttonText->SetPosition({
        baseLeft + baseWidth * 0.5f,
        baseTop + baseHeight * 0.5f + buttonOffsetsY_[animationIndex]
    });
    buttonText->SetColor(targetTextColor);
}

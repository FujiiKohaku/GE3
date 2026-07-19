#include "TitleScene.h"

#include "Engine/2D/SpriteManager.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/ImGuiManager/ImGuiManager.h"
#include "Engine/TextureManager/TextureManager.h"
#include "Engine/WinApp/WinApp.h"
#include "Engine/input/Input.h"
#include "GamePlayScene.h"
#include "LoadingScene.h"
#include "TestScene1.h"

namespace {
constexpr const char* kWhiteTexture = "resources/Textures/white.png";
constexpr const char* kGamePlayImage = "resources/Textures/Guardian.png";
constexpr const char* kTestImage = "resources/Textures/Gunner.png";

constexpr float kButtonTop = 170.0f;
constexpr float kButtonWidth = 360.0f;
constexpr float kButtonHeight = 410.0f;
constexpr float kGamePlayButtonLeft = 220.0f;
constexpr float kTestButtonLeft = 700.0f;

constexpr Vector4 kBackgroundColor = { 0.02f, 0.025f, 0.05f, 1.0f };
constexpr Vector4 kButtonColor = { 0.09f, 0.11f, 0.18f, 1.0f };
constexpr Vector4 kButtonHoverColor = { 0.18f, 0.42f, 0.68f, 1.0f };
constexpr Vector4 kHeaderColor = { 0.20f, 0.72f, 1.0f, 1.0f };
}

void TitleScene::Initialize()
{
    SetMouseCursorVisible(true);
    ClipCursor(nullptr);
    Object3dManager::GetInstance()->SetDefaultCamera(nullptr);

    TextureManager::GetInstance()->LoadTexture(kGamePlayImage);
    TextureManager::GetInstance()->LoadTexture(kTestImage);

    backgroundSprite_ = std::make_unique<Sprite>();
    backgroundSprite_->Initialize(SpriteManager::GetInstance(), kWhiteTexture);
    backgroundSprite_->SetSize({ 1280.0f, 720.0f });
    backgroundSprite_->SetColor(kBackgroundColor);

    headerLineSprite_ = std::make_unique<Sprite>();
    headerLineSprite_->Initialize(SpriteManager::GetInstance(), kWhiteTexture);
    headerLineSprite_->SetPosition({ 220.0f, 105.0f });
    headerLineSprite_->SetSize({ 840.0f, 8.0f });
    headerLineSprite_->SetColor(kHeaderColor);

    gamePlayButtonSprite_ = std::make_unique<Sprite>();
    gamePlayButtonSprite_->Initialize(SpriteManager::GetInstance(), kWhiteTexture);
    gamePlayButtonSprite_->SetPosition({ kGamePlayButtonLeft, kButtonTop });
    gamePlayButtonSprite_->SetSize({ kButtonWidth, kButtonHeight });
    gamePlayButtonSprite_->SetColor(kButtonColor);

    gamePlayImageSprite_ = std::make_unique<Sprite>();
    gamePlayImageSprite_->Initialize(SpriteManager::GetInstance(), kGamePlayImage);
    gamePlayImageSprite_->SetPosition({ kGamePlayButtonLeft + 50.0f, kButtonTop + 45.0f });
    gamePlayImageSprite_->SetSize({ 260.0f, 260.0f });

    testButtonSprite_ = std::make_unique<Sprite>();
    testButtonSprite_->Initialize(SpriteManager::GetInstance(), kWhiteTexture);
    testButtonSprite_->SetPosition({ kTestButtonLeft, kButtonTop });
    testButtonSprite_->SetSize({ kButtonWidth, kButtonHeight });
    testButtonSprite_->SetColor(kButtonColor);

    testImageSprite_ = std::make_unique<Sprite>();
    testImageSprite_->Initialize(SpriteManager::GetInstance(), kTestImage);
    testImageSprite_->SetPosition({ kTestButtonLeft + 50.0f, kButtonTop + 45.0f });
    testImageSprite_->SetSize({ 260.0f, 260.0f });

    SceneManager::GetInstance()->SetPostEffectType(PostEffectType::Copy);
}

void TitleScene::Update()
{
    const bool isGamePlayHovered = IsMouseOver(
        kGamePlayButtonLeft, kButtonTop, kButtonWidth, kButtonHeight);
    const bool isTestHovered = IsMouseOver(
        kTestButtonLeft, kButtonTop, kButtonWidth, kButtonHeight);

    gamePlayButtonSprite_->SetColor(isGamePlayHovered ? kButtonHoverColor : kButtonColor);
    testButtonSprite_->SetColor(isTestHovered ? kButtonHoverColor : kButtonColor);

    if (Input::GetInstance()->IsMouseTrigger(0)) {
        if (isGamePlayHovered) {
            SceneManager::GetInstance()->SetNextSceneWithLoading<LoadingScene, GamePlayScene>();
        } else if (isTestHovered) {
            SceneManager::GetInstance()->SetNextSceneWithLoading<LoadingScene, TestScene1>();
        }
    }

    backgroundSprite_->Update();
    headerLineSprite_->Update();
    gamePlayButtonSprite_->Update();
    gamePlayImageSprite_->Update();
    testButtonSprite_->Update();
    testImageSprite_->Update();
}

void TitleScene::Draw2D()
{
    SpriteManager::GetInstance()->PreDraw();
    backgroundSprite_->Draw();
    headerLineSprite_->Draw();
    gamePlayButtonSprite_->Draw();
    gamePlayImageSprite_->Draw();
    testButtonSprite_->Draw();
    testImageSprite_->Draw();
}

void TitleScene::Draw3D() {}
void TitleScene::DrawParticle() {}

void TitleScene::DrawImGui()
{
#ifdef USE_IMGUI
    ImGui::Begin("Title Scene");
    ImGui::Text("Click Guardian: GamePlayScene");
    ImGui::Text("Click Gunner: TestScene1");
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

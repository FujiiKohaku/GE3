#include "SpriteTestScene.h"

#include "Engine/2D/SpriteManager.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/ImGuiManager/ImGuiManager.h"
#include "Engine/PostEffect/PostEffectType.h"
#include "Engine/input/Input.h"
#include "SceneManager.h"
#include "TitleScene.h"
#include <string>

namespace {
constexpr const char* kWhiteTexture = "resources/Textures/white.png";
constexpr const char* kPreviewTexture = "resources/Textures/uvChecker.png";
constexpr Vector4 kBackgroundColor = { 0.015f, 0.02f, 0.045f, 1.0f };
constexpr Vector4 kFrameColor = { 0.08f, 0.32f, 0.52f, 1.0f };
}

void SpriteTestScene::Initialize()
{
    Object3dManager::GetInstance()->SetDefaultCamera(nullptr);
    SceneManager::GetInstance()->SetPostEffectType(PostEffectType::Copy);

    backgroundSprite_ = std::make_unique<Sprite>();
    backgroundSprite_->Initialize(SpriteManager::GetInstance(), kWhiteTexture);
    backgroundSprite_->SetSize({ 1280.0f, 720.0f });
    backgroundSprite_->SetColor(kBackgroundColor);

    previewFrameSprite_ = std::make_unique<Sprite>();
    previewFrameSprite_->Initialize(SpriteManager::GetInstance(), kWhiteTexture);
    previewFrameSprite_->SetPosition({ 330.0f, 125.0f });
    previewFrameSprite_->SetSize({ 620.0f, 470.0f });
    previewFrameSprite_->SetColor(kFrameColor);

    previewSprite_ = std::make_unique<Sprite>();
    previewSprite_->Initialize(SpriteManager::GetInstance(), kPreviewTexture);
    previewSprite_->SetPosition({ 640.0f, 360.0f });
    previewSprite_->SetSize({ 560.0f, 400.0f });
    previewSprite_->SetAnchorPoint({ 0.5f, 0.5f });

    selectedDemoIndex_ = 0;
    ApplySelectedDemo();
}

void SpriteTestScene::Finalize()
{
}

void SpriteTestScene::Update()
{
    if (Input::GetInstance()->IsKeyTrigger(DIK_ESCAPE)) {
        SceneManager::GetInstance()->SetNextScene(std::make_unique<TitleScene>());
        return;
    }

    bool canUseMouseWheel = true;
#ifdef USE_IMGUI
    canUseMouseWheel = !ImGui::GetIO().WantCaptureMouse;
#endif

    const LONG mouseWheel = Input::GetInstance()->GetMouseWheel();
    if (canUseMouseWheel && mouseWheel > 0) {
        ChangeDemo(-1);
    } else if (canUseMouseWheel && mouseWheel < 0) {
        ChangeDemo(1);
    }

    backgroundSprite_->Update();
    previewFrameSprite_->Update();
    previewSprite_->Update();
}

void SpriteTestScene::Draw2D()
{
    SpriteManager::GetInstance()->PreDraw();
    backgroundSprite_->Draw();
    previewFrameSprite_->Draw();
    previewSprite_->Draw();
}

void SpriteTestScene::Draw3D()
{
}

void SpriteTestScene::DrawParticle()
{
}

void SpriteTestScene::DrawImGui()
{
#ifdef USE_IMGUI
    ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.82f);
    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_AlwaysAutoResize;
    windowFlags |= ImGuiWindowFlags_NoMove;
    ImGui::Begin("Sprite Test Scene", nullptr, windowFlags);
    ImGui::Text("NOW: %s", previewSprite_->GetMaterialName().c_str());
    ImGui::Separator();
    ImGui::Text("Mouse Wheel: Change Sprite Shader");
    ImGui::Text("ESC: Return to Title");
    ImGui::Text(
        "Index: %u/%u",
        static_cast<unsigned int>(selectedDemoIndex_ + 1),
        static_cast<unsigned int>(materialFolders_.size()));
    ImGui::Text("Folder: %s", materialFolders_[selectedDemoIndex_]);
    ImGui::End();
#endif
}

void SpriteTestScene::ChangeDemo(int direction)
{
    if (direction < 0) {
        if (selectedDemoIndex_ == 0) {
            selectedDemoIndex_ = materialFolders_.size() - 1;
        } else {
            selectedDemoIndex_--;
        }
    } else if (direction > 0) {
        selectedDemoIndex_++;
        if (selectedDemoIndex_ >= materialFolders_.size()) {
            selectedDemoIndex_ = 0;
        }
    }

    ApplySelectedDemo();
}

void SpriteTestScene::ApplySelectedDemo()
{
    const std::string shaderDirectory =
        std::string("resources/Shaders/Sprite/") + materialFolders_[selectedDemoIndex_];
    previewSprite_->SetMaterial(shaderDirectory);
}

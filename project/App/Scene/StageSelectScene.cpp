#include "StageSelectScene.h"

#include "Engine/2D/SpriteManager.h"
#include "Engine/2D/Text/TextRenderer.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/Input/Input.h"
#include "Engine/WinApp/WinApp.h"
#include "GamePlayScene.h"
#include "LoadingScene.h"
#include "SceneManager.h"
#include "SpriteTestScene.h"
#include "TestScene1.h"
#include "TextTestScene.h"
#include "TitleScene.h"
#include <algorithm>
#include <format>

namespace {
constexpr const char* kWhiteTexture = "resources/Textures/white.png";
constexpr const char* kFont =
    "resources/Fonts/NotoSansJP/NotoSansJP-Variable.ttf";
constexpr size_t kColumns = 3;
constexpr size_t kRows = 3;
constexpr size_t kCardsPerPage = kColumns * kRows;
constexpr float kCardWidth = 320.0f;
constexpr float kCardHeight = 110.0f;
constexpr float kCardStartX = 120.0f;
constexpr float kCardStartY = 150.0f;
constexpr float kCardGapX = 40.0f;
constexpr float kCardGapY = 20.0f;
}

void StageSelectScene::Initialize()
{
    ShowCursor(TRUE);
    ClipCursor(nullptr);
    Object3dManager::GetInstance()->SetDefaultCamera(nullptr);

    StageCatalog* catalog = StageCatalog::GetInstance();
    catalog->Load();
    stages_ = catalog->GetStages();
    entries_.clear();
    cards_.clear();
    selectedIndex_ = 0;
    currentPage_ = 0;
    GetCursorPos(&lastMousePosition_);
    size_t stageNumber = 0;
    for (size_t index = 0; index < stages_.size(); ++index) {
        const auto& stage = stages_[index];
        if (stage.id == "gimmick_test") continue;
        entries_.push_back({ Destination::Stage, std::format("STAGE {:02}", ++stageNumber),
            stage.name, stage.description, stage.id });
    }
    entries_.push_back({ Destination::GameTest, "TEST / F1", "GAME TEST", "Open the gameplay test scene.", {} });
    entries_.push_back({ Destination::SpriteTest, "TEST / F2", "SPRITE TEST", "Open the sprite test scene.", {} });
    entries_.push_back({ Destination::TextTest, "TEST / F3", "TEXT TEST", "Open the text test scene.", {} });
    if (const auto* gimmickStage = catalog->Find("gimmick_test")) {
        entries_.push_back({ Destination::Stage, "TEST", "GIMMICK TEST",
            gimmickStage->description, gimmickStage->id });
    }
    entries_.push_back({ Destination::Title, "BACK / BACKSPACE", "TITLE", "Return to the title screen.", {} });

    background_ = std::make_unique<Sprite>();
    background_->Initialize(SpriteManager::GetInstance(), kWhiteTexture);
    background_->SetSize({ 1280.0f, 720.0f });
    background_->SetColor({ 0.015f, 0.025f, 0.055f, 1.0f });

    titleText_ = std::make_unique<Text>();
    titleText_->Initialize(kFont);
    titleText_->SetText("STAGE SELECT");
    titleText_->SetPosition({ 640.0f, 55.0f });
    titleText_->SetAnchorPoint({ 0.5f, 0.5f });
    titleText_->SetFontSize(42.0f);
    titleText_->SetColor({ 0.35f, 0.85f, 1.0f, 1.0f });

    descriptionText_ = std::make_unique<Text>();
    descriptionText_->Initialize(kFont);
    descriptionText_->SetPosition({ 640.0f, 585.0f });
    descriptionText_->SetAnchorPoint({ 0.5f, 0.5f });
    descriptionText_->SetFontSize(22.0f);
    descriptionText_->SetColor({ 0.85f, 0.92f, 1.0f, 1.0f });

    helpText_ = std::make_unique<Text>();
    helpText_->Initialize(kFont);
    helpText_->SetText("ARROW / WASD: SELECT    ENTER / SPACE: CONFIRM    CLICK: OPEN");
    helpText_->SetPosition({ 640.0f, 665.0f });
    helpText_->SetAnchorPoint({ 0.5f, 0.5f });
    helpText_->SetFontSize(18.0f);
    helpText_->SetColor({ 0.55f, 0.70f, 0.82f, 1.0f });

    toolsText_ = std::make_unique<Text>();
    toolsText_->Initialize(kFont);
    toolsText_->SetText(
        "TEST TOOLS    F1: GAME TEST    F2: SPRITE TEST    F3: TEXT TEST");
    toolsText_->SetPosition({ 640.0f, 625.0f });
    toolsText_->SetAnchorPoint({ 0.5f, 0.5f });
    toolsText_->SetFontSize(17.0f);
    toolsText_->SetColor({ 0.35f, 0.85f, 1.0f, 1.0f });

    cards_.reserve(entries_.size());
    for (size_t index = 0; index < entries_.size(); ++index) {
        StageCard card;
        card.background = std::make_unique<Sprite>();
        card.background->Initialize(SpriteManager::GetInstance(), kWhiteTexture);
        card.background->SetMaterial("resources/Shaders/Sprite/Border");

        card.numberText = std::make_unique<Text>();
        card.numberText->Initialize(kFont);
        card.numberText->SetText(entries_[index].label);
        card.numberText->SetFontSize(20.0f);
        card.numberText->SetColor({ 0.35f, 0.85f, 1.0f, 1.0f });

        card.nameText = std::make_unique<Text>();
        card.nameText->Initialize(kFont);
        card.nameText->SetText(entries_[index].name);
        card.nameText->SetFontSize(26.0f);
        card.nameText->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
        cards_.push_back(std::move(card));
    }
    RefreshCards();
}

void StageSelectScene::Update()
{
    Input* input = Input::GetInstance();
    if (input == nullptr) {
        return;
    }
    if (input->IsKeyTrigger(DIK_F1)) {
        SceneManager::GetInstance()->SetNextSceneWithLoading<
            LoadingScene, TestScene1>();
        return;
    }
    if (input->IsKeyTrigger(DIK_F2)) {
        SceneManager::GetInstance()->SetNextScene(
            std::make_unique<SpriteTestScene>());
        return;
    }
    if (input->IsKeyTrigger(DIK_F3)) {
        SceneManager::GetInstance()->SetNextScene(
            std::make_unique<TextTestScene>());
        return;
    }
    if (input->IsKeyTrigger(DIK_BACK)) {
        SceneManager::GetInstance()->SetNextScene(std::make_unique<TitleScene>());
        return;
    }
    if (entries_.empty()) {
        return;
    }

    size_t next = selectedIndex_;
    if (input->IsKeyTrigger(DIK_RIGHT) || input->IsKeyTrigger(DIK_D)) ++next;
    if (input->IsKeyTrigger(DIK_LEFT) || input->IsKeyTrigger(DIK_A)) {
        if (next > 0) --next;
    }
    if (input->IsKeyTrigger(DIK_DOWN) || input->IsKeyTrigger(DIK_S)) {
        next += kColumns;
    }
    if (input->IsKeyTrigger(DIK_UP) || input->IsKeyTrigger(DIK_W)) {
        next = next >= kColumns ? next - kColumns : 0;
    }
    if (next >= entries_.size()) next = entries_.size() - 1;

    POINT mousePosition {};
    GetCursorPos(&mousePosition);
    const bool mouseMoved = mousePosition.x != lastMousePosition_.x ||
        mousePosition.y != lastMousePosition_.y;
    lastMousePosition_ = mousePosition;

    const size_t pageStart = currentPage_ * kCardsPerPage;
    for (size_t visible = 0; visible < kCardsPerPage; ++visible) {
        const size_t index = pageStart + visible;
        if (index >= entries_.size()) break;
        if (IsMouseOverCard(visible)) {
            if (mouseMoved && next == selectedIndex_) next = index;
            if (input->IsMouseTrigger(0)) {
                selectedIndex_ = index;
                ActivateSelectedEntry();
                return;
            }
        }
    }

    if (next != selectedIndex_) {
        selectedIndex_ = next;
        currentPage_ = selectedIndex_ / kCardsPerPage;
        RefreshCards();
    }
    if (input->IsKeyTrigger(DIK_RETURN) || input->IsKeyTrigger(DIK_SPACE)) {
        ActivateSelectedEntry();
        return;
    }

    background_->Update();
    titleText_->Update();
    descriptionText_->Update();
    helpText_->Update();
    toolsText_->Update();
    for (StageCard& card : cards_) {
        card.background->Update();
        card.numberText->Update();
        card.nameText->Update();
    }
}

void StageSelectScene::RefreshCards()
{
    const size_t pageStart = currentPage_ * kCardsPerPage;
    for (size_t index = 0; index < cards_.size(); ++index) {
        const size_t visible = index - pageStart;
        if (index < pageStart || visible >= kCardsPerPage) continue;
        const size_t column = visible % kColumns;
        const size_t row = visible / kColumns;
        const float x = kCardStartX + column * (kCardWidth + kCardGapX);
        const float y = kCardStartY + row * (kCardHeight + kCardGapY);
        cards_[index].background->SetPosition({ x, y });
        cards_[index].background->SetSize({ kCardWidth, kCardHeight });
        cards_[index].background->SetColor(
            index == selectedIndex_
                ? Vector4 { 0.16f, 0.48f, 0.75f, 1.0f }
                : Vector4 { 0.08f, 0.12f, 0.22f, 1.0f });
        cards_[index].numberText->SetPosition({ x + 20.0f, y + 25.0f });
        cards_[index].nameText->SetPosition({
            x + kCardWidth * 0.5f,
            y + kCardHeight * 0.58f });
        cards_[index].nameText->SetAnchorPoint({ 0.5f, 0.5f });
    }
    if (selectedIndex_ < entries_.size()) {
        descriptionText_->SetText(entries_[selectedIndex_].description);
    }
}

bool StageSelectScene::IsMouseOverCard(size_t visibleIndex) const
{
    POINT mouse {};
    GetCursorPos(&mouse);
    ScreenToClient(WinApp::GetInstance()->GetHwnd(), &mouse);
    const size_t column = visibleIndex % kColumns;
    const size_t row = visibleIndex / kColumns;
    const float x = kCardStartX + column * (kCardWidth + kCardGapX);
    const float y = kCardStartY + row * (kCardHeight + kCardGapY);
    return mouse.x >= x && mouse.x <= x + kCardWidth &&
        mouse.y >= y && mouse.y <= y + kCardHeight;
}

void StageSelectScene::ActivateSelectedEntry()
{
    if (selectedIndex_ >= entries_.size()) return;
    const auto& entry = entries_[selectedIndex_];
    auto* manager = SceneManager::GetInstance();
    switch (entry.destination) {
    case Destination::Stage:
        manager->SetNextSceneWithLoading<LoadingScene, GamePlayScene>(entry.stageId);
        break;
    case Destination::GameTest:
        manager->SetNextSceneWithLoading<LoadingScene, TestScene1>();
        break;
    case Destination::SpriteTest:
        manager->SetNextScene(std::make_unique<SpriteTestScene>());
        break;
    case Destination::TextTest:
        manager->SetNextScene(std::make_unique<TextTestScene>());
        break;
    case Destination::Title:
        manager->SetNextScene(std::make_unique<TitleScene>());
        break;
    }
}

void StageSelectScene::Draw2D()
{
    SpriteManager::GetInstance()->PreDraw();
    background_->Draw();
    const size_t pageStart = currentPage_ * kCardsPerPage;
    const size_t pageEnd = std::min(pageStart + kCardsPerPage, cards_.size());
    for (size_t index = pageStart; index < pageEnd; ++index) {
        cards_[index].background->Draw();
    }
    TextRenderer::GetInstance()->PreDraw();
    titleText_->Draw();
    descriptionText_->Draw();
    helpText_->Draw();
    toolsText_->Draw();
    for (size_t index = pageStart; index < pageEnd; ++index) {
        cards_[index].numberText->Draw();
        cards_[index].nameText->Draw();
    }
}

void StageSelectScene::Finalize() {}
void StageSelectScene::Draw3D() {}
void StageSelectScene::DrawParticle() {}
void StageSelectScene::DrawImGui() {}

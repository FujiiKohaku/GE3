// チュートリアルのメッセージ・操作ボタン・達成状況をパネルとして描画する。
#include "TutorialCommPanel.h"
#include "Engine/2D/SpriteManager.h"
#include "Engine/2D/Text/TextRenderer.h"
#include "Engine/Winapp/WinApp.h"
#include <algorithm>
#include <cmath>

namespace {
constexpr Vector4 kCyan { 0.18f, 0.86f, 1.0f, 1.0f };
constexpr const char* kFont = "resources/Fonts/NotoSansJP/NotoSansJP-Variable.ttf";
}

// パネルの文字と図形を準備し、表示を開始できる状態にする。
void TutorialCommPanel::Initialize()
{
    parts_.clear();
    // 現行の最大構成は通常枠38個 + DPAD6個 + A/RT各40個。
    constexpr size_t kInitialPartCount = 124;
    parts_.reserve(kInitialPartCount);
    for (size_t index = 0; index < kInitialPartCount; ++index) {
        auto part = std::make_unique<Sprite>();
        part->Initialize(SpriteManager::GetInstance(), "resources/Textures/white.png");
        part->SetAnchorPoint({ 0.5f, 0.5f });
        parts_.push_back(std::move(part));
    }
    header_ = CreateText();
    status_ = CreateText();
    message_ = CreateText();
    operation_ = CreateText();
    buttonLabels_.clear();
    for (int i = 0; i < 3; ++i) {
        buttonLabels_.push_back(CreateText());
        buttonLabels_.back()->SetAnchorPoint({ 0.5f, 0.5f });
    }
    header_->SetText("TRAINING AI");
    status_->SetText("ONLINE");
    status_->SetAnchorPoint({ 1.0f, 0.5f });
    visible_ = false;
    compact_ = false;
    alpha_ = elapsed_ = 0.0f;
    height_ = 240.0f;
    Update(0.0f);
}

// 説明文・操作文・ボタン一覧を受け取り、現在の案内表示を更新する。
void TutorialCommPanel::ShowMessage(const std::string& message, const std::string& operation,
    const std::vector<std::string>& buttons)
{
    message_->SetText(message);
    operation_->SetText(operation);
    success_ = operation == "練習クリア";
    buttons_ = buttons;
    if (buttons_.size() > buttonLabels_.size()) buttons_.resize(buttonLabels_.size());
    if (success_) {
        status_->SetText("完了");
    } else {
        status_->SetText("練習中");
    }
    visible_ = true;
}

void TutorialCommPanel::SetProgress(int completed, int total)
{
    total_ = std::clamp(total, 1, 14);
    completed_ = std::clamp(completed, 0, total_);
}

// 表示の透明度や経過時間を進め、パネルの位置と文字・図形の配置を更新する。
void TutorialCommPanel::Update(float deltaTime)
{
    if (!header_) return;
    float dt = 0.0f;
    if (std::isfinite(deltaTime)) dt = (std::max)(0.0f, deltaTime);
    elapsed_ += dt;
    const float follow = 1.0f - std::exp(-10.0f * dt);
    float targetAlpha = 0.0f;
    if (visible_) targetAlpha = 1.0f;
    float targetHeight = 240.0f;
    if (compact_) targetHeight = 72.0f;
    alpha_ += (targetAlpha - alpha_) * follow;
    height_ += (targetHeight - height_) * follow;
    const float screenWidth = static_cast<float>(WinApp::GetInstance()->GetClientWidth());
    const float screenHeight = static_cast<float>(WinApp::GetInstance()->GetClientHeight());
    const float scale = (std::max)(0.1f, (std::min)(screenWidth / 1280.0f, screenHeight / 720.0f));
    constexpr float width = 720.0f;
    panelPosition_ = { (screenWidth - width * scale) * 0.5f,
        screenHeight - (44.0f + height_ - 12.0f * (1.0f - alpha_)) * scale };
    panelScale_ = scale;
    activePartCount_ = 0;
    const Vector4 background { 0.006f, 0.018f, 0.040f, 0.91f };
    AddRectangle(width * 0.5f, height_ * 0.5f, width - 24.0f, height_, background);
    AddRectangle(6.0f, height_ * 0.5f, 12.0f, height_ - 24.0f, background);
    AddRectangle(width - 6.0f, height_ * 0.5f, 12.0f, height_ - 24.0f, background);
    // white.pngの回転した細線で、角を切った外枠を作る。
    AddLine(12, 0, width - 12, 0, kCyan);
    AddLine(width - 12, 0, width, 12, kCyan);
    AddLine(width, 12, width, height_ - 12, kCyan);
    AddLine(width, height_ - 12, width - 12, height_, kCyan);
    AddLine(width - 12, height_, 12, height_, kCyan);
    AddLine(12, height_, 0, height_ - 12, kCyan);
    AddLine(0, height_ - 12, 0, 12, kCyan);
    AddLine(0, 12, 12, 0, kCyan);
    const Vector4 divider { 0.18f, 0.64f, 0.78f, 0.55f };
    AddLine(22, 32, width - 22, 32, divider);
    if (!compact_) {
        AddLine(22, height_ - 78, width - 22, height_ - 78, divider);
        AddLine(108, 46, 108, height_ - 88, divider);
    }
    float waveY = 74.0f;
    float waveScale = 1.0f;
    if (compact_) {
        waveY = 50.0f;
        waveScale = 0.45f;
    }
    const float waveHeights[] = { 10, 22, 34, 46, 34, 22, 10 };
    for (int i = 0; i < 7; ++i) {
        const float pulse = 0.85f + 0.15f * std::sin(elapsed_ * 5.0f + i * 0.7f);
        AddRectangle(40.0f + i * 6.0f, waveY, 3.0f,
            waveHeights[i] * pulse * waveScale, kCyan);
    }
    Vector4 accent = kCyan;
    Vector4 operationBackground { 0.025f, 0.085f, 0.14f, 1.0f };
    if (success_) {
        accent = { 0.30f, 1.0f, 0.55f, 1.0f };
        operationBackground = { 0.025f, 0.16f, 0.075f, 1.0f };
    }
    const float operationY = height_ - 43.0f;
    AddRectangle(width * 0.5f, operationY, width - 44, 58,
        operationBackground);
    AddRectangle(24, operationY, 4, 58, accent);
    if (success_) {
        AddRectangle(46.5f, operationY + 4.5f, 13.0f, 4.0f, accent, 0.785398f);
        AddRectangle(60.0f, operationY - 0.5f, 26.0f, 4.0f, accent, -0.812418f);
    }
    // 小さなUI図形として描画し、解像度に合わせてボタンの輪郭も拡大する。
    for (size_t i = 0; i < buttons_.size(); ++i) {
        const std::string& button = buttons_[i];
        const float cx = 54.0f + static_cast<float>(i) * 58.0f;
        const Vector4 rim { 0.65f, 0.76f, 0.84f, 1.0f };
        Vector4 fill { 0.08f, 0.12f, 0.17f, 1.0f };
        if (button == "A") fill = { 0.08f, 0.56f, 0.22f, 1.0f };
        if (button == "B") fill = { 0.78f, 0.13f, 0.15f, 1.0f };
        if (button == "X") fill = { 0.08f, 0.36f, 0.85f, 1.0f };
        if (button == "Y") fill = { 0.96f, 0.73f, 0.10f, 1.0f };
        if (button == "DPAD") {
            AddRectangle(cx, operationY, 14, 38, rim);
            AddRectangle(cx, operationY, 38, 14, rim);
            AddRectangle(cx, operationY, 10, 34, fill);
            AddRectangle(cx, operationY, 34, 10, fill);
            // 今回使う左右の方向を明るく示す。
            AddRectangle(cx - 13, operationY, 6, 6, kCyan);
            AddRectangle(cx + 13, operationY, 6, 6, kCyan);
        } else {
            const bool isShoulder = button == "LB" || button == "RB";
            const bool isTrigger = button == "RT";
            float buttonWidth = 40.0f;
            float buttonHeight = 40.0f;
            float radius = 20.0f;
            if (isShoulder) {
                buttonWidth = 50.0f;
                buttonHeight = 30.0f;
                radius = 8.0f;
            } else if (isTrigger) {
                buttonWidth = 34.0f;
                buttonHeight = 44.0f;
                radius = 8.0f;
            }
            AddRoundedRectangle(cx, operationY, buttonWidth, buttonHeight, radius, rim);
            AddRoundedRectangle(cx, operationY, buttonWidth - 4, buttonHeight - 4, radius - 2, fill);
            if (button == "LS") AddRoundedRectangle(cx, operationY - 4, 18, 18, 9, rim);
            if (button == "MENU") {
                for (int bar = -1; bar <= 1; ++bar) AddRectangle(cx, operationY + bar * 6, 18, 2, rim);
            }
        }
    }
    for (int i = 0; i < total_; ++i) {
        Vector4 progressColor { 0.12f, 0.30f, 0.40f, 1.0f };
        if (i < completed_) progressColor = kCyan;
        AddRectangle(width - 26.0f - (total_ - 1 - i) * 30.0f, height_ - 7.0f,
            22.0f, 4.0f, progressColor);
    }
    AddRectangle(width - 22, 17, 4, 4, kCyan);
    message_->SetMaxWidth(560.0f * scale);
    message_->SetLineSpacing(4.0f * scale);
    UpdateText(*header_, 26, 17, 13, kCyan);
    UpdateText(*status_, width - 34, 17, 14, accent);
    Vector4 messageColor { 0.90f, 0.97f, 1.0f, 1.0f };
    if (compact_) messageColor.w = 0.0f;
    UpdateText(*message_, 132, 94, 20, messageColor);
    float operationX = 40.0f;
    float operationFontSize = 20.0f;
    Vector4 operationColor { 0.96f, 0.99f, 1.0f, 1.0f };
    if (success_) {
        operationX = 86.0f;
        operationFontSize = 26.0f;
        operationColor = accent;
    } else if (!buttons_.empty()) {
        operationX = 84.0f + static_cast<float>(buttons_.size()) * 58.0f;
    }
    operation_->SetMaxWidth((width - operationX - 30) * scale);
    operation_->SetLineSpacing(2.0f * scale);
    UpdateText(*operation_, operationX, operationY, operationFontSize, operationColor);
    for (size_t i = 0; i < buttonLabels_.size(); ++i) {
        const bool shouldShowLabel = i < buttons_.size() && buttons_[i] != "DPAD" && buttons_[i] != "MENU" && buttons_[i] != "LS";
        std::string label;
        float labelFontSize = 17.0f;
        Vector4 labelColor { 1, 1, 1, 1 };
        if (shouldShowLabel) {
            label = buttons_[i];
            if (label.size() == 1) labelFontSize = 24.0f;
            if (label == "Y") labelColor = { 0.07f, 0.06f, 0.02f, 1.0f };
        }
        buttonLabels_[i]->SetText(label);
        UpdateText(*buttonLabels_[i], 54.0f + static_cast<float>(i) * 58.0f, operationY,
            labelFontSize, labelColor);
    }
}

// 現在使用しているパネル部品と文字を描画する。
void TutorialCommPanel::Draw()
{
    if (alpha_ < 0.001f) return;
    SpriteManager::GetInstance()->PreDraw();
    for (size_t index = 0; index < activePartCount_; ++index) {
        parts_[index]->Draw();
    }
    TextRenderer::GetInstance()->PreDraw();
    header_->Draw();
    status_->Draw();
    if (!compact_) message_->Draw();
    operation_->Draw();
    for (auto& label : buttonLabels_) label->Draw();
}

// パネル内で共通して使う設定のテキストを生成する。
std::unique_ptr<Text> TutorialCommPanel::CreateText() const
{
    auto text = std::make_unique<Text>();
    text->Initialize(kFont);
    text->SetAnchorPoint({ 0.0f, 0.5f });
    return text;
}

// 描画用の矩形を用意し、位置・大きさ・色を設定する。
void TutorialCommPanel::AddRectangle(float centerX, float centerY, float width, float height,
    Vector4 color, float rotation)
{
    // 未使用のSpriteは保持するだけにし、更新・描画対象に含めない。
    if (activePartCount_ == parts_.size()) {
        auto part = std::make_unique<Sprite>();
        part->Initialize(SpriteManager::GetInstance(), "resources/Textures/white.png");
        part->SetAnchorPoint({ 0.5f, 0.5f });
        parts_.push_back(std::move(part));
    }
    Sprite& sprite = *parts_[activePartCount_++];
    sprite.SetPosition({ panelPosition_.x + centerX * panelScale_, panelPosition_.y + centerY * panelScale_ });
    sprite.SetSize({ width * panelScale_, height * panelScale_ });
    sprite.SetRotation(rotation);
    color.w *= alpha_;
    sprite.SetColor(color);
    sprite.Update();
}

// 始点と終点から長さと角度を求め、細い矩形で線を表す。
void TutorialCommPanel::AddLine(float startX, float startY, float endX, float endY, Vector4 color)
{
    const float differenceX = endX - startX;
    const float differenceY = endY - startY;
    AddRectangle((startX + endX) * 0.5f, (startY + endY) * 0.5f,
        std::sqrt(differenceX * differenceX + differenceY * differenceY), 1.0f, color,
        std::atan2(differenceY, differenceX));
}

// 矩形と角の部品を組み合わせ、角を丸めたパネル形状を作る。
void TutorialCommPanel::AddRoundedRectangle(float centerX, float centerY, float width, float height,
    float radius, Vector4 color)
{
    constexpr int kRowCount = 20;
    const float rowHeight = height / kRowCount;
    for (int row = 0; row < kRowCount; ++row) {
        const float offset = -height * 0.5f + (row + 0.5f) * rowHeight;
        const float corner = (std::max)(0.0f, std::abs(offset) - (height * 0.5f - radius));
        const float inset = radius - std::sqrt((std::max)(0.0f, radius * radius - corner * corner));
        AddRectangle(centerX, centerY + offset, width - inset * 2.0f, rowHeight + 0.2f, color);
    }
}

// 文字の表示位置・サイズ・色を現在のパネル配置に揃える。
void TutorialCommPanel::UpdateText(Text& text, float x, float y, float fontSize, Vector4 color)
{
    text.SetPosition({ panelPosition_.x + x * panelScale_, panelPosition_.y + y * panelScale_ });
    text.SetFontSize(fontSize * panelScale_);
    color.w *= alpha_;
    text.SetColor(color);
    text.Update();
}

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

void TutorialCommPanel::Initialize()
{
    parts_.clear();
    for (int i = 0; i < 200; ++i) {
        auto part = std::make_unique<Sprite>();
        part->Initialize(SpriteManager::GetInstance(), "resources/Textures/white.png");
        part->SetAnchorPoint({ 0.5f, 0.5f });
        parts_.push_back(std::move(part));
    }
    const auto text = [] {
        auto result = std::make_unique<Text>();
        result->Initialize(kFont);
        result->SetAnchorPoint({ 0.0f, 0.5f });
        return result;
    };
    header_ = text();
    status_ = text();
    message_ = text();
    operation_ = text();
    buttonLabels_.clear();
    for (int i = 0; i < 3; ++i) {
        buttonLabels_.push_back(text());
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

void TutorialCommPanel::ShowMessage(const std::string& message, const std::string& operation,
    const std::vector<std::string>& buttons)
{
    message_->SetText(message);
    operation_->SetText(operation);
    success_ = operation == "練習クリア";
    buttons_ = buttons;
    if (buttons_.size() > buttonLabels_.size()) buttons_.resize(buttonLabels_.size());
    status_->SetText(success_ ? "完了" : "練習中");
    visible_ = true;
}

void TutorialCommPanel::SetProgress(int completed, int total)
{
    total_ = std::clamp(total, 1, 14);
    completed_ = std::clamp(completed, 0, total_);
}

void TutorialCommPanel::Update(float deltaTime)
{
    if (!header_) return;
    const float dt = std::isfinite(deltaTime) ? (std::max)(0.0f, deltaTime) : 0.0f;
    elapsed_ += dt;
    const float follow = 1.0f - std::exp(-10.0f * dt);
    alpha_ += ((visible_ ? 1.0f : 0.0f) - alpha_) * follow;
    height_ += ((compact_ ? 72.0f : 240.0f) - height_) * follow;
    const float screenWidth = static_cast<float>(WinApp::GetInstance()->GetClientWidth());
    const float screenHeight = static_cast<float>(WinApp::GetInstance()->GetClientHeight());
    const float scale = (std::max)(0.1f, (std::min)(screenWidth / 1280.0f, screenHeight / 720.0f));
    constexpr float width = 720.0f;
    const float x = (screenWidth - width * scale) * 0.5f;
    const float y = screenHeight - (44.0f + height_ - 12.0f * (1.0f - alpha_)) * scale;
    size_t index = 0;
    const auto rect = [&](float cx, float cy, float w, float h, Vector4 color, float rotation = 0.0f) {
        auto& sprite = parts_[index++];
        sprite->SetPosition({ x + cx * scale, y + cy * scale });
        sprite->SetSize({ w * scale, h * scale });
        sprite->SetRotation(rotation);
        color.w *= alpha_;
        sprite->SetColor(color);
        sprite->Update();
    };
    const auto line = [&](float ax, float ay, float bx, float by, Vector4 color) {
        const float dx = bx - ax, dy = by - ay;
        rect((ax + bx) * 0.5f, (ay + by) * 0.5f,
            std::sqrt(dx * dx + dy * dy), 1.0f, color, std::atan2(dy, dx));
    };
    const Vector4 background { 0.006f, 0.018f, 0.040f, 0.91f };
    rect(width * 0.5f, height_ * 0.5f, width - 24.0f, height_, background);
    rect(6.0f, height_ * 0.5f, 12.0f, height_ - 24.0f, background);
    rect(width - 6.0f, height_ * 0.5f, 12.0f, height_ - 24.0f, background);
    // white.pngの回転した細線で、角を切った外枠を作る。
    line(12, 0, width - 12, 0, kCyan);
    line(width - 12, 0, width, 12, kCyan);
    line(width, 12, width, height_ - 12, kCyan);
    line(width, height_ - 12, width - 12, height_, kCyan);
    line(width - 12, height_, 12, height_, kCyan);
    line(12, height_, 0, height_ - 12, kCyan);
    line(0, height_ - 12, 0, 12, kCyan);
    line(0, 12, 12, 0, kCyan);
    const Vector4 divider { 0.18f, 0.64f, 0.78f, 0.55f };
    line(22, 32, width - 22, 32, divider);
    if (!compact_) {
        line(22, height_ - 78, width - 22, height_ - 78, divider);
        line(108, 46, 108, height_ - 88, divider);
    }
    const float waveY = compact_ ? 50.0f : 74.0f;
    const float waveHeights[] = { 10, 22, 34, 46, 34, 22, 10 };
    for (int i = 0; i < 7; ++i) {
        const float pulse = 0.85f + 0.15f * std::sin(elapsed_ * 5.0f + i * 0.7f);
        rect(40.0f + i * 6.0f, waveY, 3.0f,
            waveHeights[i] * pulse * (compact_ ? 0.45f : 1.0f), kCyan);
    }
    const Vector4 accent = success_ ? Vector4 { 0.30f, 1.0f, 0.55f, 1.0f } : kCyan;
    const float operationY = height_ - 43.0f;
    rect(width * 0.5f, operationY, width - 44, 58,
        success_ ? Vector4 { 0.025f, 0.16f, 0.075f, 1.0f } : Vector4 { 0.025f, 0.085f, 0.14f, 1.0f });
    rect(24, operationY, 4, 58, accent);
    if (success_) {
        rect(46.5f, operationY + 4.5f, 13.0f, 4.0f, accent, 0.785398f);
        rect(60.0f, operationY - 0.5f, 26.0f, 4.0f, accent, -0.812418f);
    }
    // 小さなUI図形として描画し、解像度に合わせてボタンの輪郭も拡大する。
    const auto rounded = [&](float cx, float cy, float w, float h, float radius, Vector4 color) {
        constexpr int rows = 20;
        const float rowHeight = h / rows;
        for (int row = 0; row < rows; ++row) {
            const float offset = -h * 0.5f + (row + 0.5f) * rowHeight;
            const float corner = (std::max)(0.0f, std::abs(offset) - (h * 0.5f - radius));
            const float inset = radius - std::sqrt((std::max)(0.0f, radius * radius - corner * corner));
            rect(cx, cy + offset, w - inset * 2.0f, rowHeight + 0.2f, color);
        }
    };
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
            rect(cx, operationY, 14, 38, rim);
            rect(cx, operationY, 38, 14, rim);
            rect(cx, operationY, 10, 34, fill);
            rect(cx, operationY, 34, 10, fill);
            // 今回使う左右の方向を明るく示す。
            rect(cx - 13, operationY, 6, 6, kCyan);
            rect(cx + 13, operationY, 6, 6, kCyan);
        } else {
            const bool shoulder = button == "LB" || button == "RB";
            const bool trigger = button == "RT";
            const float w = shoulder ? 50.0f : (trigger ? 34.0f : 40.0f);
            const float h = shoulder ? 30.0f : (trigger ? 44.0f : 40.0f);
            const float radius = shoulder || trigger ? 8.0f : 20.0f;
            rounded(cx, operationY, w, h, radius, rim);
            rounded(cx, operationY, w - 4, h - 4, radius - 2, fill);
            if (button == "LS") rounded(cx, operationY - 4, 18, 18, 9, rim);
            if (button == "MENU") {
                for (int bar = -1; bar <= 1; ++bar) rect(cx, operationY + bar * 6, 18, 2, rim);
            }
        }
    }
    for (int i = 0; i < total_; ++i) {
        const bool active = i < completed_;
        rect(width - 26.0f - (total_ - 1 - i) * 30.0f, height_ - 7.0f,
            22.0f, 4.0f, active ? kCyan : Vector4 { 0.12f, 0.30f, 0.40f, 1.0f });
    }
    rect(width - 22, 17, 4, 4, kCyan);
    for (; index < parts_.size(); ++index) {
        parts_[index]->SetColor({ 0, 0, 0, 0 });
        parts_[index]->Update();
    }
    const auto updateText = [&](Text& text, float tx, float ty, float fontSize, Vector4 color) {
        text.SetPosition({ x + tx * scale, y + ty * scale });
        text.SetFontSize(fontSize * scale);
        color.w *= alpha_;
        text.SetColor(color);
        text.Update();
    };
    message_->SetMaxWidth(560.0f * scale);
    message_->SetLineSpacing(4.0f * scale);
    updateText(*header_, 26, 17, 13, kCyan);
    updateText(*status_, width - 34, 17, 14, accent);
    updateText(*message_, 132, 94, 20, { 0.90f, 0.97f, 1.0f, compact_ ? 0.0f : 1.0f });
    const float operationX = success_ ? 86.0f : (buttons_.empty() ? 40.0f : 84.0f + static_cast<float>(buttons_.size()) * 58.0f);
    operation_->SetMaxWidth((width - operationX - 30) * scale);
    operation_->SetLineSpacing(2.0f * scale);
    updateText(*operation_, operationX, operationY, success_ ? 26.0f : 20.0f,
        success_ ? accent : Vector4 { 0.96f, 0.99f, 1.0f, 1.0f });
    for (size_t i = 0; i < buttonLabels_.size(); ++i) {
        const bool show = i < buttons_.size() && buttons_[i] != "DPAD" && buttons_[i] != "MENU" && buttons_[i] != "LS";
        buttonLabels_[i]->SetText(show ? buttons_[i] : "");
        updateText(*buttonLabels_[i], 54.0f + static_cast<float>(i) * 58.0f, operationY,
            show && buttons_[i].size() == 1 ? 24.0f : 17.0f,
            show && buttons_[i] == "Y" ? Vector4 { 0.07f, 0.06f, 0.02f, 1.0f } : Vector4 { 1, 1, 1, 1 });
    }
}

void TutorialCommPanel::Draw()
{
    if (alpha_ < 0.001f) return;
    SpriteManager::GetInstance()->PreDraw();
    for (auto& part : parts_) part->Draw();
    TextRenderer::GetInstance()->PreDraw();
    header_->Draw();
    status_->Draw();
    if (!compact_) message_->Draw();
    operation_->Draw();
    for (auto& label : buttonLabels_) label->Draw();
}

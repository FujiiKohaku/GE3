#pragma once

#include "Engine/2D/Sprite.h"
#include "Engine/2D/SpriteManager.h"
#include <algorithm>
#include <memory>

namespace PageTransition {
inline bool pendingReveal = false;
inline Vector4 pendingRevealColor = { 1.0f, 0.91f, 0.68f, 1.0f };
inline float pendingRevealDuration = 0.40f;

inline void RequestReveal(
    const Vector4& color = { 1.0f, 0.91f, 0.68f, 1.0f },
    float duration = 0.40f)
{
    pendingReveal = true;
    pendingRevealColor = color;
    pendingRevealDuration = (std::max)(duration, 0.01f);
}

class RevealOverlay {
public:
    void InitializeIfRequested()
    {
        if (!pendingReveal) {
            return;
        }
        pendingReveal = false;
        elapsedTime_ = 0.0f;
        color_ = pendingRevealColor;
        duration_ = pendingRevealDuration;
        sprite_ = std::make_unique<Sprite>();
        sprite_->Initialize(
            SpriteManager::GetInstance(),
            "resources/Textures/white.png");
        sprite_->SetAnchorPoint({ 0.5f, 0.5f });
        sprite_->SetPosition({ 640.0f, 360.0f });
        sprite_->SetSize({ 1280.0f, 720.0f });
        sprite_->SetColor(color_);
        sprite_->Update();
    }

    void Update(float deltaTime)
    {
        if (!sprite_) {
            return;
        }
        elapsedTime_ += deltaTime;
        const float progress = std::clamp(elapsedTime_ / duration_, 0.0f, 1.0f);
        const float eased = progress * progress * (3.0f - 2.0f * progress);
        Vector4 fadedColor = color_;
        fadedColor.w = color_.w * (1.0f - eased);
        sprite_->SetColor(fadedColor);
        sprite_->Update();
        if (progress >= 1.0f) {
            sprite_.reset();
        }
    }

    void Draw()
    {
        if (sprite_) {
            SpriteManager::GetInstance()->PreDraw();
            sprite_->Draw();
        }
    }

private:
    std::unique_ptr<Sprite> sprite_;
    float elapsedTime_ = 0.0f;
    float duration_ = 0.40f;
    Vector4 color_ = { 1.0f, 0.91f, 0.68f, 1.0f };
};
}

#pragma once

#include "Engine/2D/Sprite.h"
#include "Engine/2D/SpriteManager.h"
#include <algorithm>
#include <memory>
#include <cmath>

namespace PageTransition {
class RevealRequest {
public:
    static RevealRequest& GetInstance() { static RevealRequest request; return request; }
    void Request(const Vector4& color, float duration)
    {
        if (!std::isfinite(duration) || !std::isfinite(color.x) ||
            !std::isfinite(color.y) || !std::isfinite(color.z) || !std::isfinite(color.w)) { return; }
        pending_ = true;
        color_ = color;
        duration_ = (std::max)(duration, 0.01f);
    }
    bool Consume(Vector4& color, float& duration)
    {
        if (!pending_) { return false; }
        pending_ = false;
        color = color_;
        duration = duration_;
        return true;
    }
private:
    RevealRequest() = default;
    RevealRequest(const RevealRequest&) = delete;
    RevealRequest& operator=(const RevealRequest&) = delete;
    bool pending_ = false;
    Vector4 color_ = { 1.0f, 0.91f, 0.68f, 1.0f };
    float duration_ = 0.40f;
};

inline void RequestReveal(
    const Vector4& color = { 1.0f, 0.91f, 0.68f, 1.0f }, float duration = 0.40f)
{
    RevealRequest::GetInstance().Request(color, duration);
}

class RevealOverlay {
public:
    void InitializeIfRequested()
    {
        if (!RevealRequest::GetInstance().Consume(color_, duration_)) {
            return;
        }
        elapsedTime_ = 0.0f;
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

#include "WaterPillarHazard.h"

#include "Engine/3D/Model.h"
#include "Engine/3D/Object3d.h"
#include "Engine/3D/Object3dManager.h"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace {
constexpr float kWarningDuration = 2.4f;
constexpr float kRisingDuration = 0.8f;
constexpr float kActiveDuration = 1.5f;
constexpr float kFadingDuration = 0.75f;

float EaseOutCubic(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    const float inverse = 1.0f - t;
    return 1.0f - inverse * inverse * inverse;
}

float EaseInCubic(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * t;
}

float SmoothStep(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
}

void WaterPillarHazard::Initialize(
    Model* planeModel,
    const Vector3& position,
    float triggerDistance,
    float delay)
{
    position_ = position;
    triggerDistance_ = triggerDistance;
    activationDelay_ = delay;

    auto createObject = [planeModel]() {
        auto object = std::make_unique<Object3d>();
        object->Initialize(Object3dManager::GetInstance());
        object->SetModel(planeModel);
        object->SetEnableLighting(false);
        return object;
    };

    warning_ = createObject();
    pillarFront_ = createObject();
    pillarSide_ = createObject();
    warning_->SetRotate({ std::numbers::pi_v<float> * 0.5f, 0.0f, 0.0f });
    pillarSide_->SetRotate({ 0.0f, std::numbers::pi_v<float> * 0.5f, 0.0f });
    ApplyVisuals();
}

void WaterPillarHazard::Update(float railDistance, float deltaTime)
{
    if (state_ == State::Waiting) {
        if (railDistance >= triggerDistance_) {
            timer_ += deltaTime;
            if (timer_ >= activationDelay_) {
                state_ = State::Warning;
                timer_ = 0.0f;
            }
        }
    } else if (state_ != State::Finished) {
        timer_ += deltaTime;
        if (state_ == State::Warning && timer_ >= kWarningDuration) {
            state_ = State::Rising; timer_ = 0.0f;
        } else if (state_ == State::Rising && timer_ >= kRisingDuration) {
            state_ = State::Active; timer_ = 0.0f;
        } else if (state_ == State::Active && timer_ >= kActiveDuration) {
            state_ = State::Fading; timer_ = 0.0f;
        } else if (state_ == State::Fading && timer_ >= kFadingDuration) {
            state_ = State::Finished; timer_ = 0.0f;
        }
    }

    ApplyVisuals();
    warning_->Update();
    pillarFront_->Update();
    pillarSide_->Update();
}

void WaterPillarHazard::ApplyVisuals()
{
    float warningScale = 0.0f;
    float pillarRatio = 0.0f;
    float alpha = 0.0f;

    if (state_ == State::Warning) {
        const float warningProgress = SmoothStep(timer_ / kWarningDuration);
        const float pulse = 0.75f + std::sin(timer_ * 18.0f) * 0.18f;
        warningScale = (2.0f + warningProgress * 5.5f) * pulse;
        const float previewProgress = SmoothStep((timer_ / kWarningDuration - 0.42f) / 0.58f);
        pillarRatio = previewProgress * 0.16f;
        alpha = previewProgress * 0.20f;
    } else if (state_ == State::Rising) {
        const float riseProgress = EaseOutCubic(timer_ / kRisingDuration);
        warningScale = 7.5f + riseProgress * 1.5f;
        pillarRatio = riseProgress;
        alpha = SmoothStep(timer_ / kRisingDuration) * 0.86f;
    } else if (state_ == State::Active) {
        pillarRatio = 1.0f;
        alpha = 0.82f;
    } else if (state_ == State::Fading) {
        const float fadeProgress = EaseInCubic(timer_ / kFadingDuration);
        pillarRatio = 1.0f - fadeProgress;
        alpha = (1.0f - SmoothStep(timer_ / kFadingDuration)) * 0.72f;
    }

    warning_->SetTranslate({ position_.x, position_.y + 0.08f, position_.z });
    warning_->SetScale({ warningScale, warningScale, 1.0f });
    const float warningAlpha = state_ == State::Warning
        ? 0.28f + SmoothStep(timer_ / kWarningDuration) * 0.42f
        : (state_ == State::Rising ? 0.42f * (1.0f - SmoothStep(timer_ / kRisingDuration)) : 0.0f);
    warning_->SetColor({ 0.65f, 0.94f, 1.0f, warningAlpha });

    const float visibleHeight = height_ * pillarRatio;
    const Vector3 pillarPosition = { position_.x, position_.y + visibleHeight * 0.5f, position_.z };
    const Vector3 pillarScale = { radius_ * 2.0f, visibleHeight, 1.0f };
    pillarFront_->SetTranslate(pillarPosition);
    pillarSide_->SetTranslate(pillarPosition);
    pillarFront_->SetScale(pillarScale);
    pillarSide_->SetScale(pillarScale);
    const Vector4 color = { 0.48f, 0.86f, 1.0f, alpha };
    pillarFront_->SetColor(color);
    pillarSide_->SetColor(color);
}

void WaterPillarHazard::Draw()
{
    if (state_ == State::Finished || state_ == State::Waiting) return;
    warning_->Draw();
    pillarFront_->Draw();
    pillarSide_->Draw();
}

bool WaterPillarHazard::CheckCollision(const Vector3& playerPosition) const
{
    if (state_ != State::Active &&
        !(state_ == State::Rising && EaseOutCubic(timer_ / kRisingDuration) >= 0.62f)) return false;
    const float dx = playerPosition.x - position_.x;
    const float dz = playerPosition.z - position_.z;
    const bool insideRadius = dx * dx + dz * dz <= radius_ * radius_;
    return insideRadius && playerPosition.y >= position_.y && playerPosition.y <= position_.y + height_;
}

bool WaterPillarHazard::IsFinished() const
{
    return state_ == State::Finished;
}

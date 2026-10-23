// レール上の進行に合わせて出現する水柱の、予告・表示・接触判定を扱う。
#include "WaterPillarHazard.h"

#include "Engine/3D/WaterPillarRenderer.h"
#include <algorithm>
#include <cmath>

namespace {
constexpr float kWarningDuration = 3.0f;
constexpr float kRisingDuration = 0.7f;
constexpr float kActiveDuration = 3.0f;
constexpr float kFadingDuration = 0.75f;
constexpr float kPreviewHeightRatio = 0.16f;

float EaseInQuart(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * t * t;
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

// 水柱の位置・出現距離・遅延時間と、描画に使う参照を記録する。
void WaterPillarHazard::Initialize(
    WaterPillarRenderer* renderer,
    const Vector3& position,
    float triggerDistance,
    float delay)
{
    position_ = position;
    triggerDistance_ = triggerDistance;
    activationDelay_ = delay;
    renderer_ = renderer;

    ApplyVisuals();
}

// 出現距離に到達してから、予告・上昇・維持・消失の順に状態を進める。
void WaterPillarHazard::Update(float railDistance, float deltaTime)
{
    if (state_ == State::Waiting) {
        if (railDistance >= triggerDistance_) {
            timer_ += deltaTime;
            if (timer_ >= activationDelay_) {
                state_ = State::Warning;
                timer_ = 0.0f;
                warningEffectHandle_ = EffectManager::GetInstance()->PlayLoopEffect(
                    "WaterPillarWarning", { position_.x, position_.y + 0.1f, position_.z });
            }
        }
    } else if (state_ != State::Finished) {
        timer_ += deltaTime;
        if (state_ == State::Warning && timer_ >= kWarningDuration) {
            if (warningEffectHandle_ != kInvalidEffectHandle) {
                EffectManager::GetInstance()->StopEffect(warningEffectHandle_);
                warningEffectHandle_ = kInvalidEffectHandle;
            }
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
}

// 状態と経過時間から水柱の高さと透明度を求め、急な出現や消失を避ける。
void WaterPillarHazard::ApplyVisuals()
{
    float pillarRatio = 0.0f;
    float alpha = 0.0f;

    if (state_ == State::Warning) {
        const float previewProgress = SmoothStep((timer_ / kWarningDuration - 0.42f) / 0.58f);
        pillarRatio = previewProgress * kPreviewHeightRatio;
        alpha = previewProgress * 0.20f;
    } else if (state_ == State::Rising) {
        const float riseProgress = EaseInQuart(timer_ / kRisingDuration);
        pillarRatio = kPreviewHeightRatio + (1.0f - kPreviewHeightRatio) * riseProgress;
        alpha = SmoothStep(timer_ / kRisingDuration) * 0.86f;
    } else if (state_ == State::Active) {
        pillarRatio = 1.0f;
        alpha = 0.82f;
    } else if (state_ == State::Fading) {
        const float fadeProgress = EaseInCubic(timer_ / kFadingDuration);
        pillarRatio = 1.0f - fadeProgress;
        alpha = (1.0f - SmoothStep(timer_ / kFadingDuration)) * 0.72f;
    }

    const float visibleHeight = height_ * pillarRatio;
    const Vector3 pillarScale = { radius_, visibleHeight, radius_ };
    pillarScale_ = pillarScale;
    const Vector4 color = { 0.48f, 0.86f, 1.0f, alpha };
    pillarColor_ = color;
}

void WaterPillarHazard::DrawPillar()
{
    if (state_ == State::Finished || state_ == State::Waiting || renderer_ == nullptr) return;
    renderer_->Draw(position_, pillarScale_, pillarColor_);
}

// 予告中は当たらないようにし、十分に上昇した水柱の半径と高さの範囲を調べる。
bool WaterPillarHazard::CheckCollision(const Vector3& playerPosition) const
{
    const float risingHeightRatio = kPreviewHeightRatio +
        (1.0f - kPreviewHeightRatio) * EaseInQuart(timer_ / kRisingDuration);
    if (state_ != State::Active &&
        !(state_ == State::Rising && risingHeightRatio >= 0.62f)) return false;
    const float dx = playerPosition.x - position_.x;
    const float dz = playerPosition.z - position_.z;
    const bool insideRadius = dx * dx + dz * dz <= radius_ * radius_;
    return insideRadius && playerPosition.y >= position_.y && playerPosition.y <= position_.y + pillarScale_.y;
}

bool WaterPillarHazard::IsFinished() const
{
    return state_ == State::Finished;
}

#include "App/Game/Enemy/Bullet/SwarmPulseBullet.h"

#include "Engine/math/MathStruct.h"
#include <cmath>

namespace {
Vector3 CrossVector(const Vector3& left, const Vector3& right)
{
    Vector3 result {};
    result.x = left.y * right.z - left.z * right.y;
    result.y = left.z * right.x - left.x * right.z;
    result.z = left.x * right.y - left.y * right.x;
    return result;
}

bool IsNearlyZero(const Vector3& value)
{
    return Vector3LengthSquared(value) < 0.000001f;
}
}

void SwarmPulseBullet::Initialize(Model* model)
{
    transform_.scale = { 0.22f, 0.22f, 0.22f };
    maxLifeTime_ = 4.0f;
    collisionRadius_ = 1.0f;
    damage_ = 1;

    EnemyBullet::Initialize(model);
    SetEnableLighting(false);
    SetColor({ 0.20f, 0.95f, 1.0f, 1.0f });
}

void SwarmPulseBullet::SetTranslate(const Vector3& translate)
{
    pathPosition_ = translate;
    EnemyBullet::SetTranslate(translate);
}

void SwarmPulseBullet::SetSwarmVelocity(const Vector3& velocity)
{
    velocity_ = velocity;

    Vector3 forward = Normalize(velocity_);
    Vector3 referenceUp = { 0.0f, 1.0f, 0.0f };
    waveSide_ = Normalize(CrossVector(referenceUp, forward));
    if (IsNearlyZero(waveSide_)) {
        waveSide_ = { 1.0f, 0.0f, 0.0f };
    }

    waveUp_ = Normalize(CrossVector(forward, waveSide_));
    if (IsNearlyZero(waveUp_)) {
        waveUp_ = referenceUp;
    }
}

void SwarmPulseBullet::SetWavePhase(float phase)
{
    wavePhase_ = phase;
}

void SwarmPulseBullet::Move()
{
    constexpr float kDeltaTime = 1.0f / 60.0f;
    waveTime_ += kDeltaTime;

    pathPosition_.x += velocity_.x;
    pathPosition_.y += velocity_.y;
    pathPosition_.z += velocity_.z;

    float waveAngle = waveTime_ * waveFrequency_ + wavePhase_;
    float sideOffset = std::sin(waveAngle) * waveAmplitude_;
    float upOffset = std::cos(waveAngle) * waveAmplitude_ * 0.55f;

    transform_.translate = pathPosition_;
    transform_.translate += waveSide_ * sideOffset;
    transform_.translate += waveUp_ * upOffset;
    transform_.rotate.z += 0.22f;
}

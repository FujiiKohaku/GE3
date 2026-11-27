#include "App/Game/Player/PlayerShield.h"
#include "Engine/3D/ModelManager.h"
#include "Engine/3D/Object3d.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/Audio/SoundManager.h"
#include "Engine/Math/MatrixMath.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <random>

namespace {
constexpr Vector3 kFrontDirection { 0.0f, 0.0f, 1.0f };
constexpr Vector3 kShieldColor { 0.45f, 0.65f, 1.8f };
constexpr float kActivationFlashSeconds = 0.18f;
constexpr float kFadeDurationSeconds = 0.14f;
constexpr float kBreakDurationSeconds = 0.54f;
constexpr float kBreakFlashSeconds = 0.07f;
constexpr float kShardFadeStartSeconds = 0.10f;
constexpr float kImpactDurationSeconds = 0.42f;
constexpr float kMinDirectionLengthSquared = 0.000001f;
constexpr float kContactHitIntervalSeconds = 1.0f;
constexpr double kNormalRecoveryDelaySeconds = 1.0;
constexpr double kBrokenRecoveryDelaySeconds = 4.0;
constexpr double kRecoverySecondsPerHit = 2.0;

}

PlayerShield::PlayerShield() = default;
PlayerShield::~PlayerShield() = default;

void PlayerShield::Initialize(Camera* camera)
{
    Reset();
    shell_ = std::make_unique<Object3d>();
    shell_->Initialize(Object3dManager::GetInstance());
    shell_->SetModel(ModelManager::GetInstance()->Load("Debug/Sphere/sphere.obj"));
    shell_->SetMaterial("resources/Shaders/Object3D/PlayerShield");
    shell_->SetVertexShaderPath("resources/Shaders/Object3D/PlayerShield/Render.VS.hlsl");
    shell_->SetTransparent(true);
    shell_->SetCastShadow(false);
    shell_->SetReceiveShadow(false);
    shell_->SetRaytracingEnabled(false);
    Model* shardModel = ModelManager::GetInstance()->Load("Player/ShieldShard.obj");
    for (BreakShard& shard : breakShards_) {
        shard.object = std::make_unique<Object3d>();
        shard.object->Initialize(Object3dManager::GetInstance());
        shard.object->SetModel(shardModel);
        shard.object->SetEnableLighting(false);
        shard.object->SetPixelShaderPath("resources/Shaders/Object3D/Unlit/Render.PS.hlsl");
        shard.object->SetTransparent(true);
        shard.object->SetTransparentDepthWrite(false);
        shard.object->SetCastShadow(false);
        shard.object->SetRaytracingEnabled(false);
    }
    SetCamera(camera);
}

void PlayerShield::Reset()
{
    impacts_ = {};
    nextImpact_ = 0;
    remainingHits_ = kMaxHits;
    recoveryDelaySeconds_ = 0.0;
    recoveryProgressSeconds_ = 0.0;
    contactCooldownSeconds_ = 0.0f;
    activeSeconds_ = 0.0f;
    fadeSeconds_ = 0.0f;
    breakRemainingSeconds_ = 0.0f;
    isActive_ = false;
    needsRelease_ = false;
}

void PlayerShield::SetCamera(Camera* camera)
{
    camera_ = camera;
    if (shell_) { shell_->SetCamera(camera); }
    for (BreakShard& shard : breakShards_) {
        if (shard.object) { shard.object->SetCamera(camera); }
    }
}

void PlayerShield::Update(bool shouldActivate, float deltaTimeSeconds)
{
    if (deltaTimeSeconds <= 0.0f) { return; }

    breakRemainingSeconds_ = (std::max)(0.0f, breakRemainingSeconds_ - deltaTimeSeconds);
    for (Impact& impact : impacts_) {
        impact.remainingSeconds_ = (std::max)(0.0f, impact.remainingSeconds_ - deltaTimeSeconds);
    }

    contactCooldownSeconds_ = (std::max)(0.0f, contactCooldownSeconds_ - deltaTimeSeconds);
    if (!shouldActivate) { needsRelease_ = false; }
    const bool wasActive = isActive_;
    isActive_ = shouldActivate && !needsRelease_ && remainingHits_ > 0;
    if (isActive_) {
        if (!wasActive) { activeSeconds_ = 0.0f; }
        activeSeconds_ += deltaTimeSeconds;
        fadeSeconds_ = kFadeDurationSeconds;
        return;
    }

    fadeSeconds_ = (std::max)(0.0f, fadeSeconds_ - deltaTimeSeconds);
    if (wasActive) { recoveryDelaySeconds_ = kNormalRecoveryDelaySeconds; }
    if (remainingHits_ >= kMaxHits) { return; }

    const double delayElapsed = (std::min)(recoveryDelaySeconds_, static_cast<double>(deltaTimeSeconds));
    recoveryDelaySeconds_ -= delayElapsed;
    recoveryProgressSeconds_ += deltaTimeSeconds - delayElapsed;
    while (remainingHits_ < kMaxHits && recoveryProgressSeconds_ >= kRecoverySecondsPerHit) {
        ++remainingHits_;
        recoveryProgressSeconds_ -= kRecoverySecondsPerHit;
    }
    if (remainingHits_ == kMaxHits) { recoveryProgressSeconds_ = 0.0; }
}

Sphere PlayerShield::GetCollisionSphere(const Vector3& playerPosition) const
{
    return { playerPosition, kRadius };
}

float PlayerShield::GetDisplayRatio() const
{
    const double recoveringHit = recoveryProgressSeconds_ / kRecoverySecondsPerHit;
    return static_cast<float>((remainingHits_ + recoveringHit) / kMaxHits);
}

void PlayerShield::OnBulletHit(const Vector3& playerPosition, const Vector3& bulletCenter, const Vector3& bulletMovement)
{
    if (!isActive_) { return; }

    Vector3 outward = bulletCenter - GetCollisionSphere(playerPosition).center;
    if (Vector3LengthSquared(outward) <= kMinDirectionLengthSquared) {
        outward = bulletMovement * -1.0f;
    }
    if (Vector3LengthSquared(outward) <= kMinDirectionLengthSquared) {
        outward = kFrontDirection;
    }
    outward = Normalize(outward);
    // Mirror the impact direction to match the inward-wound sphere mesh.
    impacts_[nextImpact_] = { { -outward.x, outward.y, outward.z }, kImpactDurationSeconds };
    nextImpact_ = (nextImpact_ + 1) % impacts_.size();
    ConsumeHit(playerPosition);
    if (shell_) { SoundManager::GetInstance()->Play("ShieldDeflect"); }
}

void PlayerShield::OnContactHit(const Vector3& playerPosition)
{
    if (contactCooldownSeconds_ > 0.0f) { return; }
    OnBulletHit(playerPosition, playerPosition, {});
    contactCooldownSeconds_ = kContactHitIntervalSeconds;
}

void PlayerShield::ConsumeHit(const Vector3& playerPosition)
{
    --remainingHits_;
    if (remainingHits_ == 0) {
        isActive_ = false;
        needsRelease_ = true;
        recoveryDelaySeconds_ = kBrokenRecoveryDelaySeconds;
        recoveryProgressSeconds_ = 0.0;
        breakRemainingSeconds_ = kBreakDurationSeconds;
        breakPosition_ = playerPosition;
        static thread_local std::mt19937 random(std::random_device{}());
        std::uniform_real_distribution<float> angle(0.0f, 2.0f * std::numbers::pi_v<float>);
        std::uniform_real_distribution<float> height(-1.0f, 1.0f);
        std::uniform_real_distribution<float> speed(9.0f, 26.0f);
        std::uniform_real_distribution<float> spin(-16.0f, 16.0f);
        std::uniform_real_distribution<float> lifetime(0.32f, kBreakDurationSeconds);
        for (BreakShard& shard : breakShards_) {
            const float azimuth = angle(random);
            const float y = height(random);
            const float horizontal = std::sqrt(1.0f - y * y);
            shard.direction = { horizontal * std::cos(azimuth), y, horizontal * std::sin(azimuth) };
            shard.rotation = { angle(random), angle(random), angle(random) };
            shard.spin = { spin(random), spin(random), spin(random) };
            shard.speed = speed(random);
            shard.lifetime = lifetime(random);
        }
    }
}

void PlayerShield::Draw(const Vector3& playerPosition)
{
    if (!shell_ || !camera_) { return; }

    Matrix4x4 impactParameters {};
    if (breakRemainingSeconds_ > 0.0f) {
        const float age = kBreakDurationSeconds - breakRemainingSeconds_;
        if (age < kBreakFlashSeconds) {
            impactParameters.m[0][3] = -1.0f - age / kBreakFlashSeconds;
            shell_->SetUvTransform(impactParameters);
            shell_->SetCustomWorldMatrix(MatrixMath::MakeAffineMatrix(
                Vector3 { -kRadius, kRadius, kRadius }, Vector3 {}, breakPosition_));
            shell_->Update();
            shell_->Draw();
        }
        for (BreakShard& shard : breakShards_) {
            if (!shard.object || age >= shard.lifetime) { continue; }
            const float fade = 1.0f - std::clamp(
                (age - kShardFadeStartSeconds) / (shard.lifetime - kShardFadeStartSeconds), 0.0f, 1.0f);
            const Vector3 position = breakPosition_ + shard.direction * (kRadius + shard.speed * age);
            shard.object->SetColor({ 0.46f, 1.35f, 1.95f, fade });
            shard.object->SetCustomWorldMatrix(MatrixMath::MakeAffineMatrix(
                Vector3 { 1.0f, 1.0f, 1.0f }, shard.rotation + shard.spin * age, position));
            shard.object->Update();
            shard.object->Draw();
        }
        return;
    }
    if (remainingHits_ == 0) { return; }
    bool hasVisibleImpact = false;
    for (size_t index = 0; index < impacts_.size(); ++index) {
        const Impact& impact = impacts_[index];
        impactParameters.m[index][0] = impact.direction_.x;
        impactParameters.m[index][1] = impact.direction_.y;
        impactParameters.m[index][2] = impact.direction_.z;
        impactParameters.m[index][3] = impact.remainingSeconds_ / kImpactDurationSeconds;
        hasVisibleImpact = hasVisibleImpact || impact.remainingSeconds_ > 0.0f;
    }
    if (fadeSeconds_ <= 0.0f && !hasVisibleImpact) { return; }
    // The UV transform rows hold the direction and remaining lifetime of each impact.
    shell_->SetUvTransform(impactParameters);

    const float activation = (std::max)(0.0f, 1.0f - activeSeconds_ / kActivationFlashSeconds);
    float brightness = 1.0f + 0.6f * activation;
    if (isActive_ && remainingHits_ == 1) {
        brightness *= 0.75f + 0.25f * std::cos(activeSeconds_ * 12.566371f);
    }
    shell_->SetColor({ kShieldColor.x * brightness, kShieldColor.y * brightness,
        kShieldColor.z * brightness, fadeSeconds_ / kFadeDurationSeconds });
    const Sphere shieldSphere = GetCollisionSphere(playerPosition);
    const float radius = shieldSphere.radius;
    shell_->SetCustomWorldMatrix(MatrixMath::MakeAffineMatrix(
        Vector3 { -radius, radius, radius }, Vector3 {}, shieldSphere.center));
    shell_->Update();
    shell_->Draw();
}

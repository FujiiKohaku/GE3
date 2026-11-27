#include "App/Game/Player/PlayerShield.h"
#include "Engine/3D/ModelManager.h"
#include "Engine/3D/Object3d.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/Audio/SoundManager.h"
#include "Engine/Math/MatrixMath.h"
#include <algorithm>
#include <cmath>

namespace {
constexpr Vector3 kFrontDirection { 0.0f, 0.0f, 1.0f };
constexpr Vector3 kShieldColor { 0.45f, 0.65f, 1.8f };
constexpr float kActivationFlashSeconds = 0.18f;
constexpr float kFadeDurationSeconds = 0.14f;
constexpr float kImpactDurationSeconds = 0.42f;
constexpr float kMinDirectionLengthSquared = 0.000001f;
constexpr double kLowChargeSeconds = 0.5;
constexpr double kChargeToleranceSeconds = 0.000001;

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
    SetCamera(camera);
}

void PlayerShield::Reset()
{
    impacts_ = {};
    nextImpact_ = 0;
    remainingSeconds_ = kCapacitySeconds;
    activeSeconds_ = 0.0f;
    fadeSeconds_ = 0.0f;
    isActive_ = false;
}

void PlayerShield::SetCamera(Camera* camera)
{
    camera_ = camera;
    if (shell_) { shell_->SetCamera(camera); }
}

void PlayerShield::Update(bool shouldActivate, float deltaTimeSeconds)
{
    if (deltaTimeSeconds <= 0.0f) { return; }

    for (Impact& impact : impacts_) {
        impact.remainingSeconds_ = (std::max)(0.0f, impact.remainingSeconds_ - deltaTimeSeconds);
    }

    const bool wasActive = isActive_;
    isActive_ = shouldActivate && remainingSeconds_ > 0.0;
    if (isActive_) {
        if (!wasActive) { activeSeconds_ = 0.0f; }
        activeSeconds_ += deltaTimeSeconds;
        remainingSeconds_ -= deltaTimeSeconds;
        if (remainingSeconds_ < kChargeToleranceSeconds) { remainingSeconds_ = 0.0; }
        fadeSeconds_ = kFadeDurationSeconds;
        return;
    }

    fadeSeconds_ = (std::max)(0.0f, fadeSeconds_ - deltaTimeSeconds);
    if (!shouldActivate) {
        remainingSeconds_ = (std::min)(kCapacitySeconds, remainingSeconds_ + deltaTimeSeconds);
    }
}

Sphere PlayerShield::GetCollisionSphere(const Vector3& playerPosition) const
{
    return { playerPosition, kRadius };
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
    SoundManager::GetInstance()->Play("ShieldDeflect");
}

void PlayerShield::Draw(const Vector3& playerPosition)
{
    if (!shell_ || !camera_) { return; }

    Matrix4x4 impactParameters {};
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
    if (isActive_ && remainingSeconds_ <= kLowChargeSeconds) {
        const float warningProgress = 1.0f - static_cast<float>(remainingSeconds_ / kLowChargeSeconds);
        brightness *= 0.75f + 0.25f * std::cos(warningProgress * 12.566371f);
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

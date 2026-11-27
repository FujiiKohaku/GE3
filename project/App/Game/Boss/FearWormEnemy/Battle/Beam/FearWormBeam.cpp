#include "FearWormBeam.h"
#include "App/Game/Player/Player.h"
#include "Engine/3D/ModelManager.h"
#include "Engine/3D/Object3d.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/Effect/EffectManager.h"
#include <algorithm>
#include <cmath>
#include "../State/FearWormTiming.h"
#include "Engine/Time/TimeManager.h"

FearWormBeam::FearWormBeam() = default;
FearWormBeam::~FearWormBeam() = default;

void FearWormBeam::Initialize()
{
    Model* model = ModelManager::GetInstance()->CreateBeamCylinder();
    const auto createLayer = [model](const Vector4& color) {
        auto object = std::make_unique<Object3d>();
        object->Initialize(Object3dManager::GetInstance());
        object->SetModel(model);
        object->SetMaterial("resources/Shaders/Object3D/FearWormBeam");
        object->SetEnableLighting(false);
        object->SetCastShadow(false);
        object->SetReceiveShadow(false);
        object->SetTransparent(true);
        object->SetColor(color);
        return object;
    };
    object_ = createLayer({ 3.5f, 2.8f, 1.8f, 0.95f });
    glowObject_ = createLayer({ 1.6f, 0.36f, 0.08f, 0.35f });
    Reset();
}

void FearWormBeam::Reset()
{
    Stop();
    position_ = {};
    direction_ = { 0.0f, 0.0f, -1.0f };
    uvScrollOffset_ = 0.0f;
}

void FearWormBeam::Stop()
{
    length_ = 0.0f;
    width_ = 0.0f;
    ClearHitState();
}

void FearWormBeam::ClearHitState()
{
    isHittingPlayer_ = false;
    hitEffectElapsedSeconds_ = 0.0f;
}

void FearWormBeam::SetDimensions(float length, float width)
{
    length_ = length;
    width_ = width;
}

void FearWormBeam::SetPose(const Vector3& position, const Vector3& direction)
{
    position_ = position;
    direction_ = direction;
}

void FearWormBeam::UpdateVisual(float deltaTimeSeconds)
{
    uvScrollOffset_ += kUvScrollSpeed * deltaTimeSeconds;
    uvScrollOffset_ = std::fmod(uvScrollOffset_, 1.0f);
    if (object_ == nullptr) {
        return;
    }

    Vector3 referenceUp = { 0.0f, 1.0f, 0.0f };
    if (std::abs(direction_.y) > 0.999f) {
        referenceUp = { 0.0f, 0.0f, 1.0f };
    }
    const Vector3 right = NormalizeSafe(Cross(referenceUp, direction_));
    const Vector3 up = Cross(direction_, right);

    Matrix4x4 rotation = MatrixMath::MakeIdentity4x4();
    rotation.m[0][0] = right.x;
    rotation.m[0][1] = right.y;
    rotation.m[0][2] = right.z;
    // 円柱の長軸はローカルY（0～1）。根元を発射位置に固定する。
    rotation.m[1][0] = direction_.x;
    rotation.m[1][1] = direction_.y;
    rotation.m[1][2] = direction_.z;
    rotation.m[2][0] = -up.x;
    rotation.m[2][1] = -up.y;
    rotation.m[2][2] = -up.z;
    const float drawLength = (std::max)(length_, kMinimumDrawLength);
    const Matrix4x4 translation = MatrixMath::MakeTranslateMatrix(position_);
    const auto updateLayer = [&](Object3d* layer, float radiusRate) {
        if (layer == nullptr) { return; }
        const float radius = width_ * 0.5f * radiusRate;
        const Matrix4x4 scale = MatrixMath::Matrix4x4MakeScaleMatrix({ radius, drawLength, radius });
        layer->SetCustomWorldMatrix(MatrixMath::Multiply(MatrixMath::Multiply(scale, rotation), translation));
        layer->SetUvTransform(MatrixMath::MakeTranslateMatrix({ 0.0f, uvScrollOffset_, 0.0f }));
        layer->Update();
    };
    updateLayer(object_.get(), 0.72f);
    updateLayer(glowObject_.get(), 1.0f);
}

bool FearWormBeam::IntersectsSphere(const Vector3& spherePosition, float sphereRadius) const
{
    if (length_ <= 0.0f || width_ <= 0.0f) {
        return false;
    }
    const Vector3 beamVector = direction_ * length_;
    const Vector3 toSphere = spherePosition - position_;
    const float beamLengthSquared = Vector3LengthSquared(beamVector);
    float projection = 0.0f;
    if (beamLengthSquared > 0.0f) {
        projection = std::clamp(Dot(toSphere, beamVector) / beamLengthSquared, 0.0f, 1.0f);
    }
    const Vector3 closestPosition = position_ + beamVector * projection;
    const float distanceSquared = Vector3LengthSquared(spherePosition - closestPosition);
    const float hitRadius = width_ * 0.5f + sphereRadius;
    return distanceSquared <= hitRadius * hitRadius;
}

void FearWormBeam::CheckCollision(Player* player)
{
    isHittingPlayer_ = false;
    if (player == nullptr || length_ <= 0.0f || width_ <= 0.0f
        || TimeManager::GetInstance()->GetDeltaTime() <= 0.0f) {
        return;
    }
    const Vector3 playerPosition = player->GetTranslate();
    if (!IntersectsSphere(playerPosition, kPlayerCollisionRadius)) {
        hitEffectElapsedSeconds_ = 0.0f;
        return;
    }
    player->ApplyDamage(kDamage);
    if (player->IsShieldActive()) {
        hitEffectElapsedSeconds_ = 0.0f;
        return;
    }
    isHittingPlayer_ = true;
    const int32_t effectCount = FearWormTiming::AdvanceInterval(hitEffectElapsedSeconds_,
        kHitEffectIntervalSeconds, TimeManager::GetInstance()->GetDeltaTime(), 1);
    if (effectCount > 0) {
        EffectManager::GetInstance()->PlayEffect("DamageHit", playerPosition);
    }
}

void FearWormBeam::Draw()
{
    if (object_ == nullptr || length_ <= 0.0f || width_ <= 0.0f) {
        return;
    }
    Object3dManager::GetInstance()->SetBlendMode(kBlendModeAdd);
    object_->Draw();
    if (glowObject_) { glowObject_->Draw(); }
    Object3dManager::GetInstance()->SetBlendMode(kBlendModeNormal);
}

void FearWormBeam::SubmitRaytracing(DxrRenderer& renderer)
{
    if (object_ != nullptr) {
        object_->SubmitRaytracing(renderer);
    }
}

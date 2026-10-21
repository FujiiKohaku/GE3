#include "FearWormBeam.h"
#include "App/Game/Player/Player.h"
#include "Engine/3D/ModelManager.h"
#include "Engine/3D/Object3d.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/Effect/EffectManager.h"
#include <algorithm>
#include <cmath>

FearWormBeam::FearWormBeam() = default;
FearWormBeam::~FearWormBeam() = default;

void FearWormBeam::Initialize()
{
    Model* model = ModelManager::GetInstance()->CreateBeamCross("resources/Textures/beam.jpg");
    object_ = std::make_unique<Object3d>();
    object_->Initialize(Object3dManager::GetInstance());
    object_->SetModel(model);
    object_->SetEnableLighting(false);
    Reset();
}

void FearWormBeam::Reset()
{
    Stop();
    position_ = {};
    direction_ = { 0.0f, 0.0f, -1.0f };
    uvScrollOffset_ = 0.0f;
    rotateAngleRadians_ = 0.0f;
}

void FearWormBeam::Stop()
{
    length_ = 0.0f;
    width_ = 0.0f;
    isHittingPlayer_ = false;
    hitEffectFrameCount_ = 0;
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
    if (uvScrollOffset_ < -1.0f) {
        uvScrollOffset_ += 1.0f;
    }
    rotateAngleRadians_ += kRotateSpeedRadiansPerSecond * deltaTimeSeconds;
    if (rotateAngleRadians_ > kPi * 2.0f) {
        rotateAngleRadians_ -= kPi * 2.0f;
    }
    if (object_ == nullptr) {
        return;
    }
    if (object_->GetMaterial() != nullptr) {
        object_->SetUvTransform(MatrixMath::MakeTranslateMatrix({ 0.0f, uvScrollOffset_, 0.0f }));
    }

    Vector3 referenceUp = { 0.0f, 1.0f, 0.0f };
    if (std::abs(direction_.y) > 0.999f) {
        referenceUp = { 0.0f, 0.0f, 1.0f };
    }
    const Vector3 right = NormalizeSafe(Cross(referenceUp, direction_));
    const Vector3 up = Cross(direction_, right);
    const float cosAngle = std::cos(rotateAngleRadians_);
    const float sinAngle = std::sin(rotateAngleRadians_);
    const Vector3 rotatedRight = right * cosAngle + up * sinAngle;
    const Vector3 rotatedUp = right * -sinAngle + up * cosAngle;

    Matrix4x4 rotation = MatrixMath::MakeIdentity4x4();
    rotation.m[0][0] = rotatedRight.x;
    rotation.m[0][1] = rotatedRight.y;
    rotation.m[0][2] = rotatedRight.z;
    rotation.m[1][0] = rotatedUp.x;
    rotation.m[1][1] = rotatedUp.y;
    rotation.m[1][2] = rotatedUp.z;
    rotation.m[2][0] = direction_.x;
    rotation.m[2][1] = direction_.y;
    rotation.m[2][2] = direction_.z;
    const float drawLength = (std::max)(length_, kMinimumDrawLength);
    const Matrix4x4 scale = MatrixMath::Matrix4x4MakeScaleMatrix({ width_, width_, drawLength });
    const Matrix4x4 translation = MatrixMath::MakeTranslateMatrix(position_);
    const Matrix4x4 world = MatrixMath::Multiply(MatrixMath::Multiply(scale, rotation), translation);
    object_->SetCustomWorldMatrix(world);
    object_->Update();
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
    if (player == nullptr || length_ <= 0.0f || width_ <= 0.0f) {
        return;
    }
    const Vector3 playerPosition = player->GetTranslate();
    if (!IntersectsSphere(playerPosition, kPlayerCollisionRadius)) {
        hitEffectFrameCount_ = 0;
        return;
    }
    isHittingPlayer_ = true;
    player->ApplyDamage(kDamage);
    hitEffectFrameCount_++;
    if (hitEffectFrameCount_ >= kHitEffectFrameInterval) {
        hitEffectFrameCount_ = 0;
        EffectManager::GetInstance()->PlayEffect("DamageHit", playerPosition);
    }
}

void FearWormBeam::Draw()
{
    if (object_ == nullptr) {
        return;
    }
    Object3dManager::GetInstance()->SetBlendMode(kBlendModeAdd);
    object_->Draw();
    Object3dManager::GetInstance()->SetBlendMode(kBlendModeNormal);
}

void FearWormBeam::SubmitRaytracing(DxrRenderer& renderer)
{
    if (object_ != nullptr) {
        object_->SubmitRaytracing(renderer);
    }
}

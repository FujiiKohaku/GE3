#include "App/Game/Player/Bullet/MissileBullet.h"

#include <cmath>

void MissileBullet::Initialize(Model* model)
{
    PlayerBullet::Initialize(model);

    damage_ = 10;

    transform_.scale = {
        scale_,
        scale_,
        scale_
    };

    CreateTrailEffect();
}

void MissileBullet::Update()
{
    PlayerBullet::Update();

    UpdateTrailPosition();

    if (!IsAlive()) {
        StopTrailEffect();
    }
}

void MissileBullet::SetDead()
{
    PlayerBullet::SetDead();

    StopTrailEffect();
}

void MissileBullet::OnHitEnemy(const Vector3& position)
{
    EffectManager::GetInstance()->PlayEffect(
        "MissileExplosion",
        position);

    EffectManager::GetInstance()->PlayEffect(
        "MissileExplosionRing",
        position);

    EffectManager::GetInstance()->PlayEffect(
        "MissileExplosionFlame",
        position);
}

void MissileBullet::CreateTrailEffect()
{
    trailPosition_ = std::make_shared<Vector3>(transform_.translate);

    UpdateTrailPosition();

    std::shared_ptr<Vector3> trailPosition = trailPosition_;

    trailEffectHandle_ = EffectManager::GetInstance()->AttachEffect(
        "MissileTrail",
        [trailPosition]() {
            return *trailPosition;
        });
}

void MissileBullet::UpdateTrailPosition()
{
    if (!trailPosition_) {
        return;
    }

    Vector3 trailPosition = transform_.translate;

    float velocityLength = std::sqrt(
        velocity_.x * velocity_.x + velocity_.y * velocity_.y + velocity_.z * velocity_.z);

    if (velocityLength > 0.001f) {
        trailPosition.x -= velocity_.x / velocityLength * 2.0f;
        trailPosition.y -= velocity_.y / velocityLength * 2.0f;
        trailPosition.z -= velocity_.z / velocityLength * 2.0f;
    }

    *trailPosition_ = trailPosition;
}

void MissileBullet::StopTrailEffect()
{
    if (trailEffectHandle_ == kInvalidEffectHandle) {
        return;
    }

    EffectManager::GetInstance()->StopEffect(
        trailEffectHandle_);

    trailEffectHandle_ = kInvalidEffectHandle;
}

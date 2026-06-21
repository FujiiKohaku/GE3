#include "ChargeBeamBullet.h"

void ChargeBeamBullet::Initialize(Model* model)
{
    Bullet::Initialize(model);

    maxLifeTime_ = 0.05f;
    transform_.scale = { 0.0f, 0.0f, 0.0f };

    if (object_ != nullptr) {
        object_->SetScale(transform_.scale);
    }
}

void ChargeBeamBullet::Update()
{
    if (!isAlive_) {
        return;
    }

    lifeTime_ += 1.0f / 60.0f;
    if (lifeTime_ >= maxLifeTime_) {
        isAlive_ = false;
    }

    UpdateAABB();
}

void ChargeBeamBullet::Draw()
{
}

void ChargeBeamBullet::Configure(int chargeLevel, const Vector3& position)
{
    chargeLevel_ = chargeLevel;

    if (chargeLevel_ < 1) {
        chargeLevel_ = 1;
    }

    if (chargeLevel_ > 3) {
        chargeLevel_ = 3;
    }

    transform_.translate = position;
    ApplyLevelParameters();
    UpdateAABB();

    if (object_ != nullptr) {
        object_->SetTranslate(transform_.translate);
        object_->SetScale(transform_.scale);
    }
}

void ChargeBeamBullet::ApplyLevelParameters()
{
    if (chargeLevel_ == 1) {
        length_ = 50.0f;
        halfWidth_ = 1.2f;
        damage_ = 4;
        return;
    }

    if (chargeLevel_ == 2) {
        length_ = 100.0f;
        halfWidth_ = 2.8f;
        damage_ = 12;
        return;
    }

    length_ = 150.0f;
    halfWidth_ = 5.5f;
    damage_ = 28;
}

void ChargeBeamBullet::UpdateAABB()
{
    aabbMin_.x = transform_.translate.x - halfWidth_;
    aabbMin_.y = transform_.translate.y - halfWidth_;
    aabbMin_.z = transform_.translate.z;

    aabbMax_.x = transform_.translate.x + halfWidth_;
    aabbMax_.y = transform_.translate.y + halfWidth_;
    aabbMax_.z = transform_.translate.z + length_;
}

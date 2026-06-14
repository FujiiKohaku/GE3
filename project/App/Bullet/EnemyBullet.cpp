#include "EnemyBullet.h"

#include "Engine/3D/Object3dManager.h"

void EnemyBullet::Initialize(Model* model)
{
    object_ = std::make_unique<Object3d>();

    object_->Initialize(
        Object3dManager::GetInstance());

    object_->SetModel(model);

    if (camera_ != nullptr) {
        object_->SetCamera(camera_);
    }
    object_->SetScale({ 0.3f, 0.3f, 0.3f });
   // object_->SetScale(transform_.scale);
    object_->SetRotate(transform_.rotate);
    object_->SetTranslate(transform_.translate);
}

void EnemyBullet::Update()
{
    if (!isAlive_) {
        return;
    }

    transform_.translate.x += velocity_.x;
    transform_.translate.y += velocity_.y;
    transform_.translate.z += velocity_.z;

    lifeTime_--;

    if (lifeTime_ <= 0) {
        isAlive_ = false;
    }

    object_->SetTranslate(transform_.translate);

    object_->Update();
}

void EnemyBullet::Draw()
{
    if (!isAlive_) {
        return;
    }

    object_->Draw();
}

void EnemyBullet::SetCamera(Camera* camera)
{
    camera_ = camera;

    if (object_ != nullptr) {
        object_->SetCamera(camera_);
    }
}

void EnemyBullet::SetTranslate(
    const Vector3& translate)
{
    transform_.translate = translate;
}

void EnemyBullet::SetVelocity(
    const Vector3& velocity)
{
    velocity_ = velocity;
}

Vector3 EnemyBullet::GetPosition() const
{
    return transform_.translate;
}

bool EnemyBullet::IsAlive() const
{
    return isAlive_;
}
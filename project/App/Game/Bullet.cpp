#include "Bullet.h"
#include "../../Engine/3D/Object3dManager.h"
#include "../../Engine/Camera/Camera.h"
#include <cassert>

void Bullet::Initialize(Model* model)
{
    assert(model != nullptr);

    object_ = std::make_unique<Object3d>();
    object_->Initialize(Object3dManager::GetInstance());
    object_->SetModel(model);

    if (camera_ != nullptr) {
        object_->SetCamera(camera_);
    }

    transform_.scale = { 0.3f, 0.3f, 0.3f };
    transform_.rotate = { 0.0f, 0.0f, 0.0f };
    transform_.translate = { 0.0f, 0.0f, 0.0f };

    object_->SetScale(transform_.scale);
    object_->SetRotate(transform_.rotate);
    object_->SetTranslate(transform_.translate);
}

void Bullet::Update()
{
    if (!isAlive_) {
        return;
    }

    transform_.translate.x += velocity_.x;
    transform_.translate.y += velocity_.y;
    transform_.translate.z += velocity_.z;

    lifeTime_ += 1.0f / 60.0f;

    if (lifeTime_ >= maxLifeTime_) {
        isAlive_ = false;
    }

    object_->SetScale(transform_.scale);
    object_->SetRotate(transform_.rotate);
    object_->SetTranslate(transform_.translate);

    object_->Update();
}

void Bullet::Draw()
{
    if (!isAlive_) {
        return;
    }

    object_->Draw();
}

void Bullet::SetCamera(Camera* camera)
{
    camera_ = camera;

    if (object_ != nullptr) {
        object_->SetCamera(camera_);
    }
}
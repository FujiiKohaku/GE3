#include "App/Game/Common/Bullet/BaseBullet.h"
#include "Engine/3D/Object3dManager.h"
#include <cassert>
void BaseBullet::Initialize(Model* model)
{
    assert(model != nullptr);
    object_ = std::make_unique<Object3d>();
    object_->Initialize(Object3dManager::GetInstance());
    object_->SetEnableLighting(true);
    object_->SetModel(model);

    if (camera_ != nullptr) {
        object_->SetCamera(camera_);
    }

    object_->SetScale(transform_.scale);
    object_->SetRotate(transform_.rotate);
    object_->SetTranslate(transform_.translate);
}

void BaseBullet::Update()
{
    if (!isAlive_) {
        return; // 弾が生存していない場合は更新処理を行わない
    }

    Move(); // 弾の移動処理を行う

    lifeTime_ += 1.0f / 60.0f; // 60FPSを想定して、1フレームあたりの時間を加算

    if (lifeTime_ >= maxLifeTime_) { // 最大寿命時間を超えた場合は弾を消滅させる
        SetDead();
    }

    object_->SetScale(transform_.scale);
    object_->SetRotate(transform_.rotate);
    object_->SetTranslate(transform_.translate);

    object_->Update();
}

void BaseBullet::Draw()
{
    if (!isAlive_) {
        return;
    }

    object_->Draw();
}

void BaseBullet::SetTranslate(const Vector3& translate)
{
    transform_.translate = translate;

    if (object_ == nullptr) {
        return;
    }

    // Spawn Sync
    object_->SetScale(transform_.scale);
    object_->SetRotate(transform_.rotate);
    object_->SetTranslate(transform_.translate);
    object_->Update();
}

void BaseBullet::Move()
{
    transform_.translate.x += velocity_.x;
    transform_.translate.y += velocity_.y;
    transform_.translate.z += velocity_.z;
}

void BaseBullet::SetDead()
{
    isAlive_ = false;
}
void BaseBullet::SetCamera(Camera* camera)
{
    camera_ = camera;

    if (object_ != nullptr) {
        object_->SetCamera(camera_);
    }
}

void BaseBullet::SetEnableLighting(bool enable)
{
    if (object_ != nullptr) {
        object_->SetEnableLighting(enable);
    }
}

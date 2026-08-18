#include "App/Game/Common/Bullet/BaseBullet.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/Time/TimeManager.h"
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

    previousPosition_ = transform_.translate;
    Move(); // 弾の移動処理を行う

    lifeTime_ += TimeManager::GetInstance()->GetDeltaTime();

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

void BaseBullet::SetScale(const Vector3& scale)
{
    transform_.scale = scale;

    if (object_ == nullptr) {
        return;
    }

    object_->SetScale(transform_.scale);
    object_->Update();
}

void BaseBullet::SetColor(const Vector4& color)
{
    if (object_ == nullptr) {
        return;
    }

    object_->SetColor(color);
    object_->Update();
}

void BaseBullet::SetTranslate(const Vector3& translate)
{
    transform_.translate = translate;
    previousPosition_ = translate;

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
    const float timeScale =
        GetTimeScale() * TimeManager::GetInstance()->GetDeltaTime() * 60.0f;
    transform_.translate.x += velocity_.x * timeScale;
    transform_.translate.y += velocity_.y * timeScale;
    transform_.translate.z += velocity_.z * timeScale;
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

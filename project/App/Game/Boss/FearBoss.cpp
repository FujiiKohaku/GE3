#include "FearBoss.h"
#include "Engine/3D/Object3d.h"
#include "Engine/3D/Object3dManager.h"
#include <cmath>

FearBoss::FearBoss() {
    // ボス中心のコライダー。半径は 4.0f
    BossCollider mainCollider;
    mainCollider.offset = { 0.0f, 0.0f, 0.0f };
    mainCollider.radius = 4.0f;
    mainCollider.hp = 10.0f;
    mainCollider.isDestroyed = false;
    colliders_.push_back(mainCollider);
}

void FearBoss::Initialize(Model* model) {
    object_ = std::make_unique<Object3d>();
    object_->Initialize(Object3dManager::GetInstance());
    object_->SetModel(model);

    position_ = { 0.0f, 0.0f, 1850.0f };
    rotate_ = { 0.0f, 0.0f, 0.0f };
    scale_ = { 5.0f, 5.0f, 5.0f }; // ボスなので大きめに設定
}

void FearBoss::Update(const Vector3& playerPosition) {
    if (IsDead()) {
        return;
    }

    // 左右に右往左往（X座標を -15.0f 〜 15.0f のサイン波で揺らす）
    timer_ += 0.03f;
    position_.x = std::sin(timer_) * 15.0f;
    position_.y = 0.0f;

    // 並走：Z座標はプレイヤーの少し前方（+60.0f）に追従
    position_.z = playerPosition.z + 60.0f;

    // オブジェクトのトランスフォームを更新
    object_->SetTranslate(position_);
    object_->SetRotate(rotate_);
    object_->SetScale(scale_);
    object_->Update();
}

void FearBoss::Draw() {
    if (IsDead()) {
        return;
    }

    if (object_) {
        object_->Draw();
    }
}

void FearBoss::ApplyDamage(float damage) {
    if (hp_ > 0) {
        hp_ -= static_cast<int>(damage);
        if (hp_ <= 0) {
            hp_ = 0;
            // 撃破時にコライダーも無効化
            for (auto& collider : colliders_) {
                collider.isDestroyed = true;
            }
        }
    }
}

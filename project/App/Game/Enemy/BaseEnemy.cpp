#include "App/Game/Enemy/BaseEnemy.h"

#include "Engine/3D/Object3dManager.h"
#include <cmath>

void BaseEnemy::Initialize(Model* model)
{
    object_ = std::make_unique<Object3d>();

    object_->Initialize(
        Object3dManager::GetInstance());
    object_->SetEnableLighting(true);
    transform_.scale = {
        2.0f,
        2.0f,
        2.0f
    };
    object_->SetModel(model);

    object_->SetScale(transform_.scale);
    object_->SetRotate(transform_.rotate);
    object_->SetTranslate(transform_.translate);
}

void BaseEnemy::Update()
{
    if (isDead_) {
        for (std::unique_ptr<EnemyBullet>& bullet : enemyBullets_) {
            bullet->Update();
        }
        for (size_t index = 0; index < enemyBullets_.size();) {
            if (enemyBullets_[index]->IsAlive() == false) {
                enemyBullets_.erase(enemyBullets_.begin() + index);
            } else {
                index = index + 1;
            }
        }
        return;
    }

    Move();

    Attack();

    UpdateAnimation();

    for (std::unique_ptr<EnemyBullet>& bullet : enemyBullets_) {
        bullet->Update();
    }

    // 死んだ敵の弾を配列から削除してクリーンアップ
    for (size_t index = 0; index < enemyBullets_.size();) {
        if (enemyBullets_[index]->IsAlive() == false) {
            enemyBullets_.erase(enemyBullets_.begin() + index);
        } else {
            index = index + 1;
        }
    }

    object_->SetScale(transform_.scale);
    object_->SetRotate(transform_.rotate);
    object_->SetTranslate(transform_.translate);

    object_->Update();
}
void BaseEnemy::Draw()
{
    if (!isDead_) {
        object_->Draw();
    }

    for (std::unique_ptr<EnemyBullet>& bullet :enemyBullets_) {

        bullet->Draw();
    }
}

void BaseEnemy::Move()
{
    if (!waypoints_.empty()) {
        float deltaTime = 1.0f / 60.0f;
        Vector3 target = waypoints_[currentWaypointIndex_];
        Vector3 currentPos = GetPosition();

        Vector3 dir = { target.x - currentPos.x, target.y - currentPos.y, target.z - currentPos.z };
        float distance = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);

        if (distance < 0.5f) {
            currentWaypointIndex_ = (currentWaypointIndex_ + 1) % waypoints_.size();
        } else {
            currentPos.x += (dir.x / distance) * moveSpeed_ * deltaTime;
            currentPos.y += (dir.y / distance) * moveSpeed_ * deltaTime;
            currentPos.z += (dir.z / distance) * moveSpeed_ * deltaTime;
            SetPosition(currentPos);
        }
    }
}

void BaseEnemy::Attack()
{
}

bool BaseEnemy::IsDead() const
{
    return isDead_;
}

Vector3 BaseEnemy::GetPosition() const
{
    return transform_.translate;
}

void BaseEnemy::SetPosition(const Vector3& position)
{
    transform_.translate = position;
}

void BaseEnemy::SetRotate(const Vector3& rotate)
{
    transform_.rotate = rotate;
}

void BaseEnemy::SetEnableLighting(bool enable)
{
    if (object_ != nullptr) {
        object_->SetEnableLighting(enable);
    }
}

void BaseEnemy::SetDead(bool isDead)
{
    if (isDead_ == isDead) {
        return;
    }

    isDead_ = isDead;

    if (isDead_) {
        OnDeath();
    }
}

void BaseEnemy::ApplyDamage(float damage)
{
    hp_ -= damage;

    OnDamage(damage);

    if (hp_ <= 0.0f) {
        SetDead(true);
    }
}

void BaseEnemy::ApplyDamageToPart(int32_t, float damage)
{
    ApplyDamage(damage);
}

void BaseEnemy::GetCollisionParts(std::vector<EnemyCollisionPart>& parts) const
{
    EnemyCollisionPart part {};
    part.position = GetPosition();
    part.radius = 3.0f;
    part.partIndex = 0;

    parts.push_back(part);
}

bool BaseEnemy::IsCollisionPartDamageable(int32_t) const
{
    return true;
}

void BaseEnemy::OnCollisionPartGuarded(int32_t, const Vector3&)
{
}

void BaseEnemy::UpdateAnimation()
{
}

void BaseEnemy::OnDamage(float)
{
}

void BaseEnemy::OnDeath()
{
}

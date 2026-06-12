#include "BaseEnemy.h"

#include "../../Engine/3D/Object3dManager.h"

void BaseEnemy::Initialize(Model* model)
{
    object_ = std::make_unique<Object3d>();

    object_->Initialize(
        Object3dManager::GetInstance());
    transform_.scale = {
        1.0f,
        1.0f,
        1.0f
    };
    object_->SetModel(model);

    object_->SetScale(transform_.scale);
    object_->SetRotate(transform_.rotate);
    object_->SetTranslate(transform_.translate);
}

void BaseEnemy::Update()
{
    Move();

    Attack();

    for (std::unique_ptr<EnemyBullet>& bullet :
        enemyBullets_) {

        bullet->Update();
    }

    object_->SetScale(transform_.scale);
    object_->SetRotate(transform_.rotate);
    object_->SetTranslate(transform_.translate);

    object_->Update();
}
void BaseEnemy::Draw()
{


    object_->Draw();

    for (std::unique_ptr<EnemyBullet>& bullet :enemyBullets_) {

        bullet->Draw();
    }
}

void BaseEnemy::Move()
{
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
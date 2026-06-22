#include "NormalEnemy.h"

#include "../Game/Player.h"

void NormalEnemy::Initialize(
    Model* model,
    Model* bulletModel,
    Player* player)
{
    BaseEnemy::Initialize(model);

    bulletModel_ = bulletModel;

    player_ = player;
}

void NormalEnemy::Update()
{
    BaseEnemy::Update();
}

void NormalEnemy::Attack()
{
    fireTimer_++;

    if (fireTimer_ >= fireInterval_) {

        FireBullet();

        fireTimer_ = 0;
    }
}

void NormalEnemy::FireBullet()
{
    if (player_ == nullptr) {
        return;
    }

    if (bulletModel_ == nullptr) {
        return;
    }

    std::unique_ptr<EnemyBullet> bullet = std::make_unique<EnemyBullet>();

    bullet->Initialize(bulletModel_);

    Vector3 playerPosition = player_->GetTranslate();

    Vector3 direction = Normalize(
        playerPosition - transform_.translate);

    Vector3 velocity;

    velocity.x = direction.x * bulletSpeed_;

    velocity.y = direction.y * bulletSpeed_;

    velocity.z = direction.z * bulletSpeed_;

    bullet->SetTranslate(transform_.translate);

    bullet->SetVelocity(velocity);

    enemyBullets_.push_back(std::move(bullet));
}
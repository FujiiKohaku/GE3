#include "PaintShooterEnemy.h"
#include "App/Game/Enemy/Bullet/PaintBullet.h"
#include "App/Game/Player/Player.h"
#include "Engine/math/MathStruct.h"

void PaintShooterEnemy::Initialize(Model* model, Model* bulletModel, Player* player)
{
    BaseEnemy::Initialize(model);
    bulletModel_ = bulletModel;
    player_ = player;
    hp_ = 2.0f; // 少し高めのHP
}

void PaintShooterEnemy::Update()
{
    if (isDead_) {
        return;
    }

    BaseEnemy::Update();

    if (player_) {
        Attack();
    }
}

void PaintShooterEnemy::Attack()
{
    fireTimer_++;
    if (fireTimer_ >= fireInterval_) {
        fireTimer_ = 0;
        FirePaintBullet();
    }
}

void PaintShooterEnemy::FirePaintBullet()
{
    if (!player_ || !bulletModel_) {
        return;
    }

    std::unique_ptr<PaintBullet> bullet = std::make_unique<PaintBullet>();
    bullet->Initialize(bulletModel_);

    Vector3 targetPos = player_->GetTranslate();
    Vector3 direction = Normalize(targetPos - transform_.translate);

    Vector3 velocity;
    velocity.x = direction.x * bulletSpeed_;
    velocity.y = direction.y * bulletSpeed_;
    velocity.z = direction.z * bulletSpeed_;

    bullet->SetTranslate(transform_.translate);
    bullet->SetVelocity(velocity);

    enemyBullets_.push_back(std::move(bullet));
}

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
    BaseEnemy::Update();
}

void PaintShooterEnemy::Attack()
{
    if (player_ == nullptr) {
        return;
    }

    Vector3 playerPosition = player_->GetTranslate();
    Vector3 toEnemy = transform_.translate - playerPosition;

    Vector3 playerVelocity = player_->GetAutomaticWorldVelocity();
    Vector3 forward = { 0.0f, 0.0f, 1.0f };
    float speedLength = Vector3Length(playerVelocity);
    if (speedLength > 0.001f) {
        forward = Normalize(playerVelocity);
    }

    float dot = toEnemy.x * forward.x + toEnemy.y * forward.y + toEnemy.z * forward.z;

    if (dot <= 0.0f) {
        return;
    }

    float distance = Vector3Length(toEnemy);

    if (distance <= 120.0f) {
        fireTimer_++;
        if (fireTimer_ >= fireInterval_) {
            fireTimer_ = 0;
            FirePaintBullet();
        }
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

    AddEnemyBullet(std::move(bullet));
}

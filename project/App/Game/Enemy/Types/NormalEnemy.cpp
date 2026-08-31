#include "App/Game/Enemy/Types/NormalEnemy.h"

#include "App/Game/Enemy/Bullet/NormalEnemyBullet.h"

#include "App/Game/Player/Player.h"
#include "Engine/math/MathStruct.h"
#include <cmath>

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
    if (player_ == nullptr) {
        return;
    }

    Vector3 playerPosition = player_->GetTranslate();
    Vector3 difference = playerPosition - transform_.translate;
    float distance = Vector3Length(difference);

    if (distance <= 100.0f) {
        fireTimer_++;

        if (fireTimer_ >= fireInterval_) {

            FireBullet();

            fireTimer_ = 0;
        }
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

    std::unique_ptr<EnemyBullet> bullet = std::make_unique<NormalEnemyBullet>();

    bullet->Initialize(bulletModel_);

    Vector3 playerPosition = player_->GetTranslate();

    // 弾の到達時間からPlayerの自動前進位置を予測する。
    // 65%だけ先読みし、上下左右へ動けば避けられる余地は残す。
    constexpr float kPredictionRatio = 0.65f;
    const float distance = Vector3Length(
        playerPosition - transform_.translate);
    if (bulletSpeed_ > 0.0f) {
        const float flightFrames = distance / bulletSpeed_;
        playerPosition += player_->GetAutomaticWorldVelocity() *
            (flightFrames * kPredictionRatio);
    }

    Vector3 direction = Normalize(
        playerPosition - transform_.translate);

    Vector3 velocity;

    velocity.x = direction.x * bulletSpeed_;

    velocity.y = direction.y * bulletSpeed_;

    velocity.z = direction.z * bulletSpeed_;

    bullet->SetTranslate(transform_.translate);

    bullet->SetVelocity(velocity);

    AddEnemyBullet(std::move(bullet));
}

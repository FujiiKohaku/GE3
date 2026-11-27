// プレイヤーに向けてペイント弾を発射する敵を扱う。
#include "App/Game/Common/GameTiming.h"
#include "App/Game/Enemy/EnemyShootingRange.h"
#include "Engine/Time/TimeManager.h"
#include "PaintShooterEnemy.h"
#include "App/Game/Enemy/Bullet/Types/PaintBullet.h"
#include "App/Game/Player/Player.h"
#include "Engine/math/MathStruct.h"

// 共通の敵を初期化し、ペイント弾の発射に使うモデルと狙う相手を設定する。
void PaintShooterEnemy::Initialize(Model* model, Model* bulletModel, Player* player)
{
    BaseEnemy::Initialize(model);
    bulletModel_ = bulletModel;
    player_ = player;
    fireElapsedSeconds_ = 0.0f;
    SetHealth(7.0f);
}

// 共通の更新経路から移動とペイント射撃を進める。
void PaintShooterEnemy::Update()
{
    BaseEnemy::Update();
}

// 射程や発射間隔を確認し、プレイヤーへペイント弾を発射する。
void PaintShooterEnemy::Attack()
{
    if (player_ == nullptr) {
        return;
    }

    Vector3 playerPosition = player_->GetTranslate();
    Vector3 toEnemy = transform_.translate - playerPosition;

    if (!IsEnemyAhead(toEnemy, player_->GetFlightForward())) {
        return;
    }

    float distance = Vector3Length(toEnemy);

    if (distance <= kEnemyShootingRange) {
        const int32_t shotCount = GameTiming::AdvanceInterval(fireElapsedSeconds_,
            kFireIntervalSeconds, TimeManager::GetInstance()->GetDeltaTime(), 3);
        for (int32_t shotIndex = 0; shotIndex < shotCount; shotIndex++) {
            FirePaintBullet();
        }
    }
}

// プレイヤーを狙う方向を求め、ペイント弾を敵弾管理へ渡す。
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

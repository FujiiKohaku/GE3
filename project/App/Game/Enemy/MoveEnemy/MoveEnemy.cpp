// 移動パターンに沿って位置を変えながら、プレイヤーに向けて射撃する敵を扱う。
#include "MoveEnemy.h"
#include "App/Game/Common/GameTiming.h"

#include "App/Game/Enemy/Bullet/Types/NormalEnemyBullet.h"
#include "App/Game/Player/Player.h"
#include "Engine/math/MathStruct.h"
#include "Engine/Time/TimeManager.h"
#include <cmath>

// 移動の開始位置とパターン、射撃用のモデルやプレイヤー参照を準備する。
void MoveEnemy::Initialize(Model* model, Model* bulletModel, Player* player)
{
    BaseEnemy::Initialize(model);
    SetHealth(4.0f);

    bulletModel_ = bulletModel;
    player_ = player;
    fireElapsedSeconds_ = 0.0f;

    // デフォルトパラメータの設定 (三項演算子、auto、ラムダ式は使用禁止)
    SetMovePattern(MovePattern::LeftRight);
    moveSpeed_ = 2.0f;
    amplitude_ = 5.0f;
    frequency_ = 2.0f;
    moveTime_ = 0.0f;
    isStartPositionInitialized_ = false;
    startPosition_ = transform_.translate;
}

// 基底クラスを通じて、この敵の移動・射撃・見た目を更新する。
void MoveEnemy::Update()
{
    // 親クラスの更新処理を呼び出す
    BaseEnemy::Update();
}

// 選択された移動パターンに応じて、開始位置からの変位を求める。
Vector3 MoveEnemy::CalculateMovementPosition(MovePattern pattern, const Vector3& startPosition,
    float elapsedSeconds, float speed, float amplitude, float frequency)
{
    Vector3 position = startPosition;
    switch (pattern) {
    case MovePattern::UpDown:
        position.y += std::sin(elapsedSeconds * speed) * amplitude;
        break;
    case MovePattern::ZigZag:
        position.x += std::sin(elapsedSeconds * speed) * amplitude;
        position.y += std::sin(elapsedSeconds * speed * 2.0f) * amplitude * 0.5f;
        break;
    case MovePattern::SineWave:
        position.y += std::sin(elapsedSeconds * frequency) * amplitude;
        position.z -= elapsedSeconds * speed * 1.5f;
        break;
    case MovePattern::LeftRight:
    default:
        position.x += std::sin(elapsedSeconds * speed) * amplitude;
        break;
    }
    return position;
}

// 経過時間を進め、選択した軌道上の位置へ本体を移動する。
void MoveEnemy::Move()
{
    if (isStartPositionInitialized_ == false) {
        startPosition_ = transform_.translate;
        isStartPositionInitialized_ = true;
    }

    moveTime_ += TimeManager::GetInstance()->GetDeltaTime();

    transform_.translate = CalculateMovementPosition(
        movePattern_, startPosition_, moveTime_, moveSpeed_, amplitude_, frequency_);
}
// プレイヤーとの距離や向きを確認し、射撃可能な場合だけ発射タイマーを進める。
void MoveEnemy::Attack()
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
        const int32_t shotCount = GameTiming::AdvanceInterval(fireElapsedSeconds_,
            kFireIntervalSeconds, TimeManager::GetInstance()->GetDeltaTime(), 3);
        for (int32_t shotIndex = 0; shotIndex < shotCount; shotIndex++) {
            FireBullet();
        }
    }
}

// 現在位置からプレイヤーを狙う弾を生成する。
void MoveEnemy::FireBullet()
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
    Vector3 direction = Normalize(playerPosition - transform_.translate);
    Vector3 velocity;

    velocity.x = direction.x * bulletSpeed_;
    velocity.y = direction.y * bulletSpeed_;
    velocity.z = direction.z * bulletSpeed_;

    bullet->SetTranslate(transform_.translate);
    bullet->SetVelocity(velocity);

    AddEnemyBullet(std::move(bullet));
}

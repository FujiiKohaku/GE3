#include "MoveEnemy.h"

#include "App/Game/Enemy/Bullet/NormalEnemyBullet.h"
#include "App/Game/Player/Player.h"
#include "Engine/math/MathStruct.h"
#include "externals/imgui/imgui.h"
#include <cmath>

void MoveEnemy::Initialize(Model* model, Model* bulletModel, Player* player)
{
    BaseEnemy::Initialize(model);

    bulletModel_ = bulletModel;
    player_ = player;

    // デフォルトパラメータの設定 (三項演算子、auto、ラムダ式は使用禁止)
    movePattern_ = MovePattern::LeftRight;
    moveSpeed_ = 2.0f;
    amplitude_ = 5.0f;
    frequency_ = 2.0f;
    moveTime_ = 0.0f;
    isStartPositionInitialized_ = false;
    startPosition_ = transform_.translate;
}

void MoveEnemy::Update()
{
    // 親クラスの更新処理を呼び出す
    BaseEnemy::Update();
}

void MoveEnemy::Move()
{
    if (isStartPositionInitialized_ == false) {
        startPosition_ = transform_.translate;
        isStartPositionInitialized_ = true;
    }

    moveTime_ = moveTime_ + (1.0f / 60.0f);

    // 各移動パターンによる座標計算 (累積誤差が発生しないよう startPosition_ を基準に計算)
    if (movePattern_ == MovePattern::LeftRight) {
        transform_.translate.x = startPosition_.x + std::sin(moveTime_ * moveSpeed_) * amplitude_;
        transform_.translate.y = startPosition_.y;
        transform_.translate.z = startPosition_.z;
    }
    
    if (movePattern_ == MovePattern::UpDown) {
        transform_.translate.x = startPosition_.x;
        transform_.translate.y = startPosition_.y + std::sin(moveTime_ * moveSpeed_) * amplitude_;
        transform_.translate.z = startPosition_.z;
    }
    
    if (movePattern_ == MovePattern::ZigZag) {
        // X方向は通常の往復、Y方向は倍の周波数で往復させることで滑らかなジグザグ移動を実現
        transform_.translate.x = startPosition_.x + std::sin(moveTime_ * moveSpeed_) * amplitude_;
        transform_.translate.y = startPosition_.y + std::sin(moveTime_ * moveSpeed_ * 2.0f) * amplitude_ * 0.5f;
        transform_.translate.z = startPosition_.z;
    }
    
    if (movePattern_ == MovePattern::SineWave) {
        // Z負方向へ前進しながら、Y方向へ滑らかなSin波移動
        transform_.translate.x = startPosition_.x;
        transform_.translate.y = startPosition_.y + std::sin(moveTime_ * frequency_) * amplitude_;
        transform_.translate.z = startPosition_.z - (moveTime_ * moveSpeed_ * 1.5f);
    }
}

void MoveEnemy::Attack()
{
    if (player_ == nullptr) {
        return;
    }

    Vector3 playerPosition = player_->GetTranslate();
    Vector3 difference = playerPosition - transform_.translate;
    float distance = Vector3Length(difference);

    if (distance <= 100.0f) {
        fireTimer_ = fireTimer_ + 1;

        if (fireTimer_ >= fireInterval_) {
            FireBullet();
            fireTimer_ = 0;
        }
    }
}

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

    enemyBullets_.push_back(std::move(bullet));
}

void MoveEnemy::DrawImGui()
{
    // 移動パターンのリアルタイム調整用UI (auto、三項演算子、ラムダ式は不使用)
    const char* patterns[] = { "LeftRight", "UpDown", "ZigZag", "SineWave" };
    int currentPattern = static_cast<int>(movePattern_);
    
    if (ImGui::Combo("Move Pattern", &currentPattern, patterns, 4)) {
        movePattern_ = static_cast<MovePattern>(currentPattern);
    }
    
    ImGui::SliderFloat("Move Speed", &moveSpeed_, 0.0f, 10.0f);
    ImGui::SliderFloat("Amplitude", &amplitude_, 0.0f, 20.0f);
    ImGui::SliderFloat("Frequency", &frequency_, 0.0f, 10.0f);
    
    if (ImGui::Button("Reset Position")) {
        moveTime_ = 0.0f;
        transform_.translate = startPosition_;
    }
}

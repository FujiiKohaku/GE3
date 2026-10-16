// 生成された敵弾を所有し、更新・描画・不要な弾の削除をまとめて行う。
#include "EnemyBulletManager.h"

#include <algorithm>
#include <utility>

namespace {
bool IsDeadBullet(const std::unique_ptr<EnemyBullet>& bullet)
{
    return !bullet->IsAlive();
}
}

// 敵弾を霧の影響から除外する設定を保持する。
void EnemyBulletManager::SetFogExclusionEnabled(bool isEnabled)
{
    fogExclusionEnabled_ = isEnabled;
}

// 新しい敵弾の所有権を受け取り、管理対象へ追加する。
void EnemyBulletManager::Add(std::unique_ptr<EnemyBullet> bullet)
{
    if (bullet) {
        if (fogExclusionEnabled_) {
            bullet->SetFogEnabled(false);
        }
        bullets_.push_back(std::move(bullet));
    }
}

// 敵弾を移動させ、寿命やプレイヤーとの位置関係から不要になった弾を削除する。
void EnemyBulletManager::Update(const Vector3& playerPosition, const Vector3& playerForward)
{
    constexpr float kBulletCullDistanceBehindPlayer = 120.0f;
    for (std::unique_ptr<EnemyBullet>& bullet : bullets_) {
        bullet->Update();
        const Vector3 playerToBullet = bullet->GetPosition() - playerPosition;
        const float forwardDistance = Dot(playerToBullet, playerForward);
        if (forwardDistance < -kBulletCullDistanceBehindPlayer) {
            bullet->SetDead();
        }
    }
    std::erase_if(bullets_, IsDeadBullet);
}

// 管理中の敵弾を順に描画する。
void EnemyBulletManager::Draw()
{
    for (std::unique_ptr<EnemyBullet>& bullet : bullets_) {
        bullet->Draw();
    }
}

// 当たり判定用に敵弾一覧を参照させる。弾の所有権は管理側に残す。
const std::vector<std::unique_ptr<EnemyBullet>>& EnemyBulletManager::GetBullets() const
{
    return bullets_;
}

// 管理している敵弾をすべて破棄する。
void EnemyBulletManager::Clear()
{
    bullets_.clear();
}

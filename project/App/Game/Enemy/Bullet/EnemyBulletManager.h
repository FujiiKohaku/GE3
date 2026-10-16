// 生成された敵弾を所有し、更新・描画・不要な弾の削除をまとめて行う。
#pragma once

#include "App/Game/Enemy/Bullet/Common/EnemyBullet.h"

#include <memory>
#include <vector>

class EnemyBulletManager {
public:
    // 敵弾を霧の影響から除外する設定を保持する。
    void SetFogExclusionEnabled(bool isEnabled);
    // 新しい敵弾の所有権を受け取り、管理対象へ追加する。
    void Add(std::unique_ptr<EnemyBullet> bullet);

    // 敵弾を移動させ、寿命やプレイヤーとの位置関係から不要になった弾を削除する。
    void Update(
        const Vector3& playerPosition,
        const Vector3& playerForward);

    // 管理中の敵弾を順に描画する。
    void Draw();
    // 当たり判定用に敵弾一覧を参照させる。弾の所有権は管理側に残す。
    const std::vector<std::unique_ptr<EnemyBullet>>& GetBullets() const;
    // 管理している敵弾をすべて破棄する。
    void Clear();

private:
    bool fogExclusionEnabled_ = false;
    std::vector<std::unique_ptr<EnemyBullet>> bullets_;
};

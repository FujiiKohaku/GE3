// プレイヤー弾に共通する敵への命中処理を定義し、敵弾とは別の型として扱う。
#pragma once
#include "App/Game/Common/Bullet/BaseBullet.h"

struct PlayerBulletExplosionSettings {
    float radius = 0.0f;
    float damage = 0.0f;
};

class PlayerBullet : public BaseBullet {
public:
    virtual ~PlayerBullet() = default;
    // 弾種固有の命中演出を追加するための共通の呼び出し口。標準では何も行わない。
    virtual void OnHitEnemy(const Vector3& position);
    // 半径が0なら爆発しない。衝突したフレームにだけ爆風を判定する。
    virtual PlayerBulletExplosionSettings GetExplosionSettings() const { return {}; }
};

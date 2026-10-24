// プレイヤー弾に共通する敵への命中処理を定義し、敵弾とは別の型として扱う。
#pragma once
#include "App/Game/Common/Bullet/BaseBullet.h"
class PlayerBullet : public BaseBullet {
public:
    virtual ~PlayerBullet() = default;
    // 弾種固有の命中演出を追加するための共通の呼び出し口。標準では何も行わない。
    virtual void OnHitEnemy(const Vector3& position);
};

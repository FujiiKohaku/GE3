// 敵弾に共通する描画設定と、プレイヤーへの命中時に返す演出の種類を定義する。
#pragma once

#include "App/Game/Common/Bullet/BaseBullet.h"

enum class EnemyBulletHitEffect { None, Paint };

class EnemyBullet : public BaseBullet {
public:
    virtual ~EnemyBullet() = default;

    // 共通の弾を初期化し、敵弾の材質や霧設定を適用する。
    void Initialize(Model* model) override;

    // 標準の敵弾では追加の画面演出を要求しない。
    virtual EnemyBulletHitEffect OnHitPlayer(const Vector3& position);

};

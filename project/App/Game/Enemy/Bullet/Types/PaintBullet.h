// プレイヤーへの命中時に、画面のペイント演出を要求する敵弾を定義する。
#pragma once

#include "App/Game/Enemy/Bullet/Common/EnemyBullet.h"

class PaintBullet : public EnemyBullet {
public:
    PaintBullet() = default;
    ~PaintBullet() override = default;

    // ペイント弾の大きさ・色・威力と描画設定を適用する。
    void Initialize(Model* model) override;
    // 命中をペイント弾として通知し、シーン側の画面汚れ演出につなげる。
    EnemyBulletHitEffect OnHitPlayer(const Vector3& position) override;
};

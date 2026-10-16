// プレイヤーの通常弾の描画設定と、敵への命中時のエフェクトを扱う。
#pragma once
#include "App/Game/Player/Bullet/Common/PlayerBullet.h"
class NormalBullet : public PlayerBullet {
public:
    // 通常弾の色・シェーダー・霧や影の設定を適用し、機体から見やすい弾にする。
    void Initialize(Model* model) override;

    // 共通の移動と寿命処理を使い、通常弾を進める。
    void Update() override;

    // 通常弾の命中位置へヒットエフェクトを出す。
    void OnHitEnemy(const Vector3& position) override;
};

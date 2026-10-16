// 一定時間プレイヤーを追尾し、その後は現在の方向へ進む敵弾を扱う。
#pragma once

#include "App/Game/Enemy/Bullet/Common/EnemyBullet.h"

class Player;

class TrackingEnemyBullet : public EnemyBullet {
public:
    // 追尾時間や旋回に使う状態を初期化し、追尾弾の見た目を設定する。
    void Initialize(Model* model) override;
    // 追尾の経過時間を進め、通常の弾更新へつなげる。
    void Update() override;

    // プレイヤーのポインタを設定
    void SetPlayer(Player* player) { player_ = player; }

protected:
    // 追尾可能な間は狙いへ方向を曲げ、追尾終了後は現在の速度方向へ進む。
    void Move() override;

private:
    Player* player_ = nullptr;
    float launchTimer_ = 0.0f;        // 打ち上げフェーズの経過時間
    float trackingTimer_ = 0.0f;      // 追尾フェーズの経過時間
    bool isTracking_ = false;         // 追尾状態に入ったか
    bool isPassed_ = false;           // プレイヤーを通り過ぎたか
};

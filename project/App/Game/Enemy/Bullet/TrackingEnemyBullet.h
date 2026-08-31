#pragma once

#include "App/Game/Enemy/Bullet/EnemyBullet.h"

class Player;

class TrackingEnemyBullet : public EnemyBullet {
public:
    void Initialize(Model* model) override;
    void Update() override;

    // プレイヤーのポインタを設定
    void SetPlayer(Player* player) { player_ = player; }

protected:
    void Move() override;

private:
    Player* player_ = nullptr;
    float launchTimer_ = 0.0f;        // 打ち上げフェーズの経過時間
    float trackingTimer_ = 0.0f;      // 追尾フェーズの経過時間
    bool isTracking_ = false;         // 追尾状態に入ったか
    bool isPassed_ = false;           // プレイヤーを通り過ぎたか
};

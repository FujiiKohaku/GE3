// 特殊な追尾を行わず、与えられた速度で進む通常の敵弾を定義する。
#pragma once

#include "App/Game/Enemy/Bullet/Common/EnemyBullet.h"

class NormalEnemyBullet : public EnemyBullet {
public:
    // 通常の敵弾として使う色・大きさ・描画設定を適用する。
    void Initialize(Model* model) override;
};

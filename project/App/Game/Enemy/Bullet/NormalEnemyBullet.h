#pragma once

#include "App/Game/Enemy/Bullet/EnemyBullet.h"

class NormalEnemyBullet : public EnemyBullet {
public:
    void Initialize(Model* model) override;
};

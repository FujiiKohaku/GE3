#pragma once

#include "App/Game/Enemy/Bullet/EnemyBullet.h"

class PaintBullet : public EnemyBullet {
public:
    PaintBullet() = default;
    ~PaintBullet() override = default;

    void Initialize(Model* model) override;
    void OnHitPlayer(const Vector3& position) override;
};

#pragma once

#include "App/Game/Common/Bullet/BaseBullet.h"

class EnemyBullet : public BaseBullet {
public:
    virtual ~EnemyBullet() = default;

    void Initialize(Model* model) override;

    virtual void OnHitPlayer(const Vector3& position);
};

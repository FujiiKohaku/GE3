#pragma once

#include "App/Game/Common/Bullet/BaseBullet.h"

enum class EnemyBulletHitEffect { None, Paint };

class EnemyBullet : public BaseBullet {
public:
    virtual ~EnemyBullet() = default;

    void Initialize(Model* model) override;

    virtual EnemyBulletHitEffect OnHitPlayer(const Vector3& position);

};

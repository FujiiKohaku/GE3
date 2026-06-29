#pragma once
#include "App/Game/Common/Bullet/BaseBullet.h"
class PlayerBullet : public BaseBullet {
public:
    virtual ~PlayerBullet() = default;
    virtual void OnHitEnemy(const Vector3& position);
};

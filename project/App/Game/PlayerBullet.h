#pragma once
#include "BaseBullet.h"
class PlayerBullet : public BaseBullet {
public:
    virtual ~PlayerBullet() = default;
    virtual void OnHitEnemy(const Vector3& position);
};

#pragma once

#include "App/Game/Player/Bullet/MissileBullet.h"
#include <memory>
#include <vector>

class BaseEnemy;

class HomingMissileBullet : public MissileBullet {
public:
    void SetTarget(BaseEnemy* target, const std::shared_ptr<std::vector<BaseEnemy*>>& activeTargets);

protected:
    void Move() override;

private:
    BaseEnemy* target_ = nullptr;
    std::weak_ptr<std::vector<BaseEnemy*>> activeTargets_;
    float homingStrength_ = 0.08f;
};

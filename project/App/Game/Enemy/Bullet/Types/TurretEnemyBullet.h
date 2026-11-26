#pragma once

#include "App/Game/Enemy/Bullet/Common/EnemyBullet.h"

// 通常弾より大きく速い、砲台専用の直進弾。
class TurretEnemyBullet : public EnemyBullet {
public:
    static constexpr float kDisplayScale = 0.45f;
    static constexpr float kCollisionRadius = 2.25f;
    static constexpr float kSpeedMultiplier = 5.0f;

    TurretEnemyBullet();
    void Initialize(Model* model) override;
    void SetLaunchDirection(const Vector3& direction, float normalSpeedPerFrame);
};

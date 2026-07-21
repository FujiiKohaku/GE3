#pragma once

#include "App/Game/Enemy/BaseEnemy.h"

class Player;
class Model;

class PaintShooterEnemy : public BaseEnemy {
public:
    PaintShooterEnemy() = default;
    ~PaintShooterEnemy() override = default;

    void Initialize(
        Model* model,
        Model* bulletModel,
        Player* player);

    void Update() override;

private:
    void Attack() override;
    void FirePaintBullet();

private:
    Player* player_ = nullptr;
    Model* bulletModel_ = nullptr;

    int32_t fireTimer_ = 0;
    int32_t fireInterval_ = 90; // 発射間隔
    float bulletSpeed_ = 0.85f;
};

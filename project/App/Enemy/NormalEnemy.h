#pragma once

#include "BaseEnemy.h"

class Player;
class Model;

class NormalEnemy : public BaseEnemy {
public:
    void Initialize(
        Model* model,
        Model* bulletModel,
        Player* player);

    void Update() override;

private:
    void Attack() override;
    void FireBullet();

private:
    Player* player_ = nullptr;

    Model* bulletModel_ = nullptr;

    int32_t fireTimer_ = 0;

    int32_t fireInterval_ = 60;

    float bulletSpeed_ = 0.3f;
};
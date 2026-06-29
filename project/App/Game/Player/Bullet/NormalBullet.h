#pragma once
#include "App/Game/Player/Bullet/PlayerBullet.h"
class NormalBullet : public PlayerBullet {
public:
    void Initialize(Model* model) override;

    void Update() override;

    void OnHitEnemy(const Vector3& position) override;
};

#include "App/Game/Enemy/Bullet/NormalEnemyBullet.h"

void NormalEnemyBullet::Initialize(Model* model)
{
    transform_.scale = { 0.3f, 0.3f, 0.3f };
    maxLifeTime_ = 5.0f;
    collisionRadius_ = 1.5f;

    EnemyBullet::Initialize(model);
}

#include "App/Game/Enemy/Bullet/EnemyBullet.h"

void EnemyBullet::Initialize(Model* model)
{
    BaseBullet::Initialize(model);
}

void EnemyBullet::OnHitPlayer(const Vector3&)
{
}

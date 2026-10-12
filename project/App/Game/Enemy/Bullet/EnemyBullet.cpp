#include "App/Game/Enemy/Bullet/EnemyBullet.h"

void EnemyBullet::Initialize(Model* model)
{
    BaseBullet::Initialize(model);
    SetEnableLighting(false);
    SetColor({ 1.0f, 0.353f, 0.239f, 1.0f });
}

EnemyBulletHitEffect EnemyBullet::OnHitPlayer(const Vector3&)
{
    return EnemyBulletHitEffect::None;
}

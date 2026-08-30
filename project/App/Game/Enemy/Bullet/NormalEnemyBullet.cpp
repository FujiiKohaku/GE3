#include "App/Game/Enemy/Bullet/NormalEnemyBullet.h"

void NormalEnemyBullet::Initialize(Model* model)
{
    transform_.scale = { 0.3f, 0.3f, 0.3f };
    // 画面内で時間切れにならないよう長めに確保する。
    // 通常はEnemyBulletManager側でプレイヤー後方へ抜けた時点で破棄される。
    maxLifeTime_ = 20.0f;
    collisionRadius_ = 1.5f;

    EnemyBullet::Initialize(model);
}

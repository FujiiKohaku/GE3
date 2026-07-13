#include "App/Game/Player/Bullet/NormalBullet.h"
#include "Engine/Effect/EffectManager.h"
void NormalBullet::Initialize(Model* model)
{
    // ここでは、BaseBulletのInitialize関数を呼び出して、モデルの設定やオブジェクトの初期化を行います。
    BaseBullet::Initialize(model);
    transform_.scale = { 0.3f, 0.3f, 0.3f };
    transform_.rotate = { 0.0f, 0.0f, 0.0f };
    transform_.translate = { 0.0f, 0.0f, 0.0f };
}

void NormalBullet::Update()
{
    BaseBullet::Update();
}
void NormalBullet::OnHitEnemy(const Vector3& position)
{
    EffectManager::GetInstance()->PlayEffect(
        "NormalBulletHit",
        position);

    EffectManager::GetInstance()->PlayEffect(
        "NormalBulletImpactFlash",
        position);
}

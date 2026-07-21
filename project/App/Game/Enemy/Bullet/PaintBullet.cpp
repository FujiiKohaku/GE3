#include "PaintBullet.h"

void PaintBullet::Initialize(Model* model)
{
    EnemyBullet::Initialize(model);
    // ペイント弾用に少し大きめのサイズ設定
    transform_.scale = { 0.6f, 0.6f, 0.6f };
}

void PaintBullet::OnHitPlayer(const Vector3& position)
{
    EnemyBullet::OnHitPlayer(position);
}

#include "PaintBullet.h"

void PaintBullet::Initialize(Model* model)
{
    // プレイヤー後方へ抜けるまでは残し、最長時間だけ安全装置として設ける。
    maxLifeTime_ = 20.0f;
    EnemyBullet::Initialize(model);
    // ペイント弾用に少し大きめのサイズ設定
    transform_.scale = { 0.6f, 0.6f, 0.6f };

    // 敵攻撃は危険色の朱赤へ統一し、暗い場所でも色が沈まないようにする。
    SetEnableLighting(false);
    SetColor({ 1.0f, 0.353f, 0.239f, 1.0f });
}

void PaintBullet::OnHitPlayer(const Vector3& position)
{
    EnemyBullet::OnHitPlayer(position);
}

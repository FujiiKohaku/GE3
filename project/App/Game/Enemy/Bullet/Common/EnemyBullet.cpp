// 敵弾に共通する描画設定と、プレイヤーへの命中時に返す演出の種類を定義する。
#include "App/Game/Enemy/Bullet/Common/EnemyBullet.h"

// 共通の弾を初期化し、敵弾の材質や霧設定を適用する。
void EnemyBullet::Initialize(Model* model)
{
    BaseBullet::Initialize(model);
    SetEnableLighting(false);
    SetColor({ 1.0f, 0.0f, 0.0f, 1.0f });
}

// 標準の敵弾では追加の画面演出を要求しない。
EnemyBulletHitEffect EnemyBullet::OnHitPlayer(const Vector3&)
{
    return EnemyBulletHitEffect::None;
}

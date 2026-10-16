// プレイヤーへの命中時に、画面のペイント演出を要求する敵弾を定義する。
#include "PaintBullet.h"

// ペイント弾の大きさ・色・威力と描画設定を適用する。
void PaintBullet::Initialize(Model* model)
{
    // プレイヤー後方へ抜けるまでは残し、最長時間だけ安全装置として設ける。
    SetMaxLifeTime(20.0f);
    EnemyBullet::Initialize(model);
    // ペイント弾用に少し大きめのサイズ設定
    transform_.scale = { 0.6f, 0.6f, 0.6f };

    // 敵攻撃は危険色の朱赤へ統一し、暗い場所でも色が沈まないようにする。
    SetEnableLighting(false);
    SetPixelShaderPath("resources/Shaders/Object3D/DanmakuBullet/Render.PS.hlsl");
    SetTransparent(true);
    SetTransparentDepthWrite(false);
    SetCastShadow(false);
    SetColor({ 1.0f, 0.353f, 0.239f, 1.0f });
}

// 命中をペイント弾として通知し、シーン側の画面汚れ演出につなげる。
EnemyBulletHitEffect PaintBullet::OnHitPlayer(const Vector3& position)
{
    EnemyBullet::OnHitPlayer(position);
    return EnemyBulletHitEffect::Paint;
}

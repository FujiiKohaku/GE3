// プレイヤーの通常弾の描画設定と、敵への命中時のエフェクトを扱う。
#include "App/Game/Player/Combat/Bullet/Types/NormalBullet.h"
#include "Engine/Effect/EffectManager.h"
// 通常弾の色・シェーダー・霧や影の設定を適用し、機体から見やすい弾にする。
void NormalBullet::Initialize(Model* model)
{
    // ここでは、BaseBulletのInitialize関数を呼び出して、モデルの設定やオブジェクトの初期化を行います。
    BaseBullet::Initialize(model);
    SetEnableLighting(false);
    SetColor({ 0.12f, 0.48f, 1.0f, 1.0f });
    SetPixelShaderPath("resources/Shaders/Object3D/PlayerBullet/Render.PS.hlsl");
    SetFogEnabled(false);
    SetTransparent(true);
    SetTransparentDepthWrite(false);
    SetCastShadow(false);
    transform_.scale = { 0.3f, 0.3f, 0.3f };
    transform_.rotate = { 0.0f, 0.0f, 0.0f };
    transform_.translate = { 0.0f, 0.0f, 0.0f };
}

// 共通の移動と寿命処理を使い、通常弾を進める。
void NormalBullet::Update()
{
    BaseBullet::Update();
}
// 通常弾の命中位置へヒットエフェクトを出す。
void NormalBullet::OnHitEnemy(const Vector3& position)
{
    EffectManager::GetInstance()->PlayEffect(
        "NormalBulletHit",
        position);

    EffectManager::GetInstance()->PlayEffect(
        "NormalBulletImpactFlash",
        position);
}

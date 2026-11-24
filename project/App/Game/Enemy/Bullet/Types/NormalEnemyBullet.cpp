// 特殊な追尾を行わず、与えられた速度で進む通常の敵弾を定義する。
#include "App/Game/Enemy/Bullet/Types/NormalEnemyBullet.h"

namespace {
constexpr Vector4 kNormalEnemyBulletColor { 0.48f, 0.0f, 0.0f, 1.0f };
constexpr Vector3 kNormalEnemyBulletRenderScale { 0.70f, 0.70f, 0.70f };
}

// 通常の敵弾として使う色・大きさ・描画設定を適用する。
void NormalEnemyBullet::Initialize(Model* model)
{
    transform_.scale = kNormalEnemyBulletRenderScale;
    // 画面内で時間切れにならないよう長めに確保する。
    // 通常はEnemyBulletManager側でプレイヤー後方へ抜けた時点で破棄される。
    SetMaxLifeTime(20.0f);
    SetCollisionRadius(1.5f);

    EnemyBullet::Initialize(model);

    // 東方風弾幕シェーダーの設定
    SetEnableLighting(false);
    SetPixelShaderPath("resources/Shaders/Object3D/DanmakuBullet/Render.PS.hlsl");
    SetTransparent(true);
    SetTransparentDepthWrite(false);
    SetCastShadow(false);
    SetColor(kNormalEnemyBulletColor);
}

void NormalEnemyBullet::ResetForReuse(Model* model)
{
    BaseBullet::ResetForReuse(model);
    transform_.scale = kNormalEnemyBulletRenderScale;
    SetMaxLifeTime(20.0f);
    SetCollisionRadius(1.5f);
    SetEnableLighting(false);
    SetPixelShaderPath("resources/Shaders/Object3D/DanmakuBullet/Render.PS.hlsl");
    SetTransparent(true);
    SetTransparentDepthWrite(false);
    SetCastShadow(false);
    SetColor(kNormalEnemyBulletColor);
    SetTranslate({});
}

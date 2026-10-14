#include "App/Game/Enemy/Bullet/NormalEnemyBullet.h"

void NormalEnemyBullet::Initialize(Model* model)
{
    transform_.scale = { 0.3f, 0.3f, 0.3f };
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
    SetColor({ 1.0f, 0.22f, 0.28f, 1.0f });
}

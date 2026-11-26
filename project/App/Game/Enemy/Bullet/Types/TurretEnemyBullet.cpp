#include "TurretEnemyBullet.h"

TurretEnemyBullet::TurretEnemyBullet()
{
    transform_ = {};
    velocity_ = {};
    SetScale({ kDisplayScale, kDisplayScale, kDisplayScale });
    SetCollisionRadius(kCollisionRadius);
    SetMaxLifeTime(20.0f);
}

void TurretEnemyBullet::Initialize(Model* model)
{
    EnemyBullet::Initialize(model);
    SetEnableLighting(false);
    SetPixelShaderPath("resources/Shaders/Object3D/TurretBullet/Render.PS.hlsl");
    SetTransparent(true);
    SetTransparentDepthWrite(false);
    SetCastShadow(false);
    SetColor({ 0.65f, 0.12f, 1.0f, 1.0f });
    trailStyle_ = { { 0.65f, 0.20f, 1.0f, 0.65f }, 16.0f, 0.38f };
}

void TurretEnemyBullet::SetLaunchDirection(const Vector3& direction, float normalSpeedPerFrame)
{
    const float directionLength = Vector3Length(direction);
    if (!std::isfinite(directionLength) || directionLength <= 0.0001f ||
        !std::isfinite(normalSpeedPerFrame) || normalSpeedPerFrame <= 0.0f) {
        SetVelocity({});
        return;
    }
    SetVelocity(direction * (normalSpeedPerFrame * kSpeedMultiplier / directionLength));
}

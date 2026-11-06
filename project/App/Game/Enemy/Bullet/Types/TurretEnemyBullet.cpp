#include "TurretEnemyBullet.h"
#include "App/Game/Enemy/TurretEnemy/TurretShotFeedback.h"
#include "Engine/3D/ModelManager.h"
#include "Engine/3D/Object3dManager.h"

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
    trailObject_ = std::make_unique<Object3d>();
    trailObject_->Initialize(Object3dManager::GetInstance());
    trailObject_->SetModel(ModelManager::GetInstance()->Load("Enemy/Turret/TurretTrail.obj"));
    trailObject_->SetPixelShaderPath("resources/Shaders/Object3D/TurretTrail/Render.PS.hlsl");
    trailObject_->SetEnableLighting(false);
    trailObject_->SetTransparent(true);
    trailObject_->SetTransparentDepthWrite(false);
    trailObject_->SetCastShadow(false);
    trailObject_->SetReceiveShadow(false);
    trailObject_->SetFogEnabled(false);
    trailObject_->SetColor({ 0.65f, 0.12f, 1.0f, 0.85f });
    travelDistance_ = 0.0f;
}

void TurretEnemyBullet::Update()
{
    EnemyBullet::Update();
    if (!IsAlive() || trailObject_ == nullptr || Vector3Length(velocity_) < 0.0001f) { return; }
    travelDistance_ += Vector3Length(GetPosition() - GetPreviousPosition());
    const float length = (std::min)(travelDistance_, kTrailLength);
    trailObject_->SetCustomWorldMatrix(TurretShotFeedback::CalculateTrailWorld(
        GetPosition(), Normalize(velocity_), length, kTrailWidth));
    trailObject_->Update();
}

void TurretEnemyBullet::Draw()
{
    if (!IsAlive()) { return; }
    if (trailObject_ != nullptr && travelDistance_ > 0.001f) { trailObject_->Draw(); }
    EnemyBullet::Draw();
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

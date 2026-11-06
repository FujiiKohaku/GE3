#include "StrafeEnemy.h"
#include "App/Game/Enemy/Bullet/Types/NormalEnemyBullet.h"
#include "App/Game/Player/Player.h"
#include "Engine/Time/TimeManager.h"
#include "Engine/Logger/Logger.h"

namespace {
constexpr float kPassedDistance = 30.0f;
constexpr float kTeleportDistance = 240.0f;
constexpr float kAttackRange = 170.0f;
}

void StrafeEnemy::Initialize(Model* model, Model* bulletModel, Player* player, const NormalEnemySettings& settings)
{
    transform_ = {};
    BaseEnemy::Initialize(model);
    NormalEnemySettings appliedSettings = settings;
    if (!IsValidNormalEnemySettings(appliedSettings)) { appliedSettings = {}; }
    InitializeHealth(appliedSettings.hp);
    bulletSpeedPerFrame_ = appliedSettings.bulletSpeedPerFrame;
    player_ = player;
    bulletModel_ = bulletModel;
    settings_ = {};
    transform_.scale = { 4.0f, 2.5f, 4.0f };
    object_->SetScale(transform_.scale);
    object_->SetColor({ 1.0f, 0.65f, 0.08f, 1.0f });
    object_->SetNormalMapStrength(0.0f);
    elapsedSeconds_ = 0.0f;
    scheduledShotsConsumed_ = 0;
    hasStartedFlight_ = false;
}

void StrafeEnemy::ConfigureFlight(const LevelData::StrafeEnemySpawnData& settings)
{
    settings_ = settings;
    if (!StrafeEnemyPath::IsValidSettings(settings_)) {
        Logger::Warning("Invalid StrafeEnemy settings; using defaults");
        settings_ = {};
    }
}

void StrafeEnemy::SetPosition(const Vector3& position)
{
    BaseEnemy::SetPosition(position);
    anchorPosition_ = position;
    elapsedSeconds_ = 0.0f;
    scheduledShotsConsumed_ = 0;
    hasStartedFlight_ = false;
}

void StrafeEnemy::UpdateRailFrame()
{
    railForward_ = Normalize(player_->GetFlightForward());
    if (IsNearlyZero(railForward_)) { railForward_ = { 0.0f, 0.0f, 1.0f }; }
    railRight_ = Normalize(Cross(Vector3 { 0.0f, 1.0f, 0.0f }, railForward_));
    if (IsNearlyZero(railRight_)) { railRight_ = { 1.0f, 0.0f, 0.0f }; }
    railUp_ = Normalize(Cross(railForward_, railRight_));
    const Vector3& railOffset = player_->GetRailOffset();
    railOrigin_ = player_->GetTranslate() - railRight_ * railOffset.x - railUp_ * railOffset.y - railForward_ * railOffset.z;
}

void StrafeEnemy::Update()
{
    if (IsDead() || player_ == nullptr || player_->IsDead()) { return; }
    const float deltaSeconds = TimeManager::GetInstance()->GetDeltaTime();
    if (!std::isfinite(deltaSeconds) || deltaSeconds <= 0.0f) { return; }
    const Vector3 previousRailOrigin = railOrigin_;
    UpdateRailFrame();
    if (!hasStartedFlight_) {
        const float forwardDistance = Dot(anchorPosition_ - railOrigin_, railForward_);
        if (forwardDistance < -kPassedDistance) { SetDead(true); return; }
        if (forwardDistance > settings_.activationDistance) { return; }
        hasStartedFlight_ = true;
        std::string entrySide = "right";
        if (settings_.isEnteringFromLeft) { entrySide = "left"; }
        Logger::Log("[StrafeEnemy] Flight started: entrySide=" + entrySide);
    } else if (Vector3Length(railOrigin_ - previousRailOrigin) > kTeleportDistance) {
        SetDead(true);
        return;
    }
    elapsedSeconds_ += deltaSeconds;
    if (StrafeEnemyPath::Sample(elapsedSeconds_, settings_).hasFinished) {
        Logger::Log("[StrafeEnemy] Flight completed");
        SetDead(true);
        return;
    }
    BaseEnemy::Update();
}

void StrafeEnemy::Move()
{
    const auto sample = StrafeEnemyPath::Sample(elapsedSeconds_, settings_);
    transform_.translate = railOrigin_ + railRight_ * sample.position.x +
        railUp_ * sample.position.y + railForward_ * sample.position.z;
    Vector3 direction = railRight_ * -1.0f;
    if (settings_.isEnteringFromLeft) { direction = railRight_; }
    transform_.rotate = { 0.0f, -std::atan2(direction.x, direction.z), 0.0f };
}

void StrafeEnemy::Attack()
{
    const int expectedShotCount = StrafeEnemyPath::GetScheduledShotCount(elapsedSeconds_, settings_);
    if (expectedShotCount <= scheduledShotsConsumed_) { return; }
    // 長いフレームや射程外で過ぎた射撃を、後からまとめて発射しない。
    scheduledShotsConsumed_ = expectedShotCount;
    if (bulletModel_ == nullptr || player_ == nullptr || player_->IsDead()) { return; }
    const Vector3 toPlayer = player_->GetTranslate() - transform_.translate;
    const float distance = Vector3Length(toPlayer);
    if (distance <= 0.001f || distance > kAttackRange ||
        Dot(transform_.translate - player_->GetTranslate(), railForward_) <= 0.0f) { return; }
    std::unique_ptr<NormalEnemyBullet> bullet = std::make_unique<NormalEnemyBullet>();
    bullet->Initialize(bulletModel_);
    bullet->SetTranslate(transform_.translate);
    bullet->SetVelocity(toPlayer * (bulletSpeedPerFrame_ / distance));
    AddEnemyBullet(std::move(bullet));
}

void StrafeEnemy::Draw()
{
    if (hasStartedFlight_) { BaseEnemy::Draw(); }
}

void StrafeEnemy::DrawShadow(ShadowMapRenderer& renderer)
{
    if (hasStartedFlight_) { BaseEnemy::DrawShadow(renderer); }
}

void StrafeEnemy::SubmitRaytracing(DxrRenderer& renderer)
{
    if (hasStartedFlight_) { BaseEnemy::SubmitRaytracing(renderer); }
}

void StrafeEnemy::GetCollisionParts(std::vector<EnemyCollisionPart>& parts) const
{
    if (!hasStartedFlight_ || IsDead()) { return; }
    EnemyCollisionPart part;
    part.position = transform_.translate;
    part.isBox = true;
    const Vector3 size { std::abs(transform_.scale.x), std::abs(transform_.scale.y), std::abs(transform_.scale.z) };
    part.box = CollisionManager::MakeOBB(transform_.translate, size, transform_.rotate);
    part.radius = Vector3Length(size) * 0.5f;
    parts.push_back(part);
}

#include "LoopEnemy.h"

#include "App/Game/Player/Player.h"
#include "Engine/Logger/Logger.h"
#include "Engine/Time/TimeManager.h"
#include <cmath>

namespace {
constexpr float kPassedDistance = 30.0f;
constexpr float kTeleportDistance = 240.0f;
}

void LoopEnemy::Initialize(Model* model, Model* bulletModel, Player* player)
{
    transform_ = {};
    NormalEnemy::Initialize(model, bulletModel, player);
    SetRadialShotEnabled(false);
    player_ = player;
    elapsedSeconds_ = 0.0f;
    spawnDelaySeconds_ = 0.0f;
    activationDistance_ = 120.0f;
    isEnteringFromLeft_ = false;
    hasTriggeredFlight_ = false;
    hasStartedFlight_ = false;
    hasEscaped_ = false;
    transform_.scale = { 2.4f, 1.8f, 4.2f };
    object_->SetScale(transform_.scale);
    object_->SetColor({ 1.0f, 0.48f, 0.12f, 1.0f });
    object_->SetNormalMapStrength(0.0f);
}

void LoopEnemy::SetPosition(const Vector3& position)
{
    BaseEnemy::SetPosition(position);
    anchorPosition_ = position;
    elapsedSeconds_ = -spawnDelaySeconds_;
    hasTriggeredFlight_ = false;
    hasStartedFlight_ = false;
    hasEscaped_ = false;
}

void LoopEnemy::ConfigureFlight(bool isEnteringFromLeft, float spawnDelaySeconds, float activationDistance)
{
    isEnteringFromLeft_ = isEnteringFromLeft;
    spawnDelaySeconds_ = std::clamp(spawnDelaySeconds, 0.0f, 150.0f);
    activationDistance_ = std::clamp(activationDistance, 40.0f, 150.0f);
    elapsedSeconds_ = -spawnDelaySeconds_;
}

void LoopEnemy::UpdateRailFrame()
{
    railForward_ = Normalize(player_->GetFlightForward());
    if (IsNearlyZero(railForward_)) { railForward_ = { 0.0f, 0.0f, 1.0f }; }
    railRight_ = Normalize(Cross({ 0.0f, 1.0f, 0.0f }, railForward_));
    if (IsNearlyZero(railRight_)) { railRight_ = { 1.0f, 0.0f, 0.0f }; }
    railUp_ = Normalize(Cross(railForward_, railRight_));
    const Vector3& railOffset = player_->GetRailOffset();
    railOrigin_ = player_->GetTranslate() - railRight_ * railOffset.x - railUp_ * railOffset.y - railForward_ * railOffset.z;
}

void LoopEnemy::Update()
{
    if (IsDead() || player_ == nullptr || player_->IsDead()) { return; }
    const Vector3 previousRailOrigin = railOrigin_;
    UpdateRailFrame();
    if (!hasTriggeredFlight_) {
        const Vector3 offset = anchorPosition_ - railOrigin_;
        const float forwardDistance = Dot(offset, railForward_);
        if (forwardDistance < -kPassedDistance) {
            hasEscaped_ = true;
            SetDead(true);
            return;
        }
        if (forwardDistance > activationDistance_) { return; }
        centerOffset_ = { Dot(offset, railRight_), Dot(offset, railUp_), forwardDistance };
        hasTriggeredFlight_ = true;
    } else if (Vector3Length(railOrigin_ - previousRailOrigin) > kTeleportDistance) {
        // ワープやレベル再読み込み時に、離れた敵を自機の前へ連れ戻さない。
        hasEscaped_ = true;
        SetDead(true);
        return;
    }
    const float deltaSeconds = TimeManager::GetInstance()->GetDeltaTime();
    if (!std::isfinite(deltaSeconds) || deltaSeconds <= 0.0f) { return; }
    elapsedSeconds_ += deltaSeconds;
    if (elapsedSeconds_ < 0.0f) { return; }
    if (!hasStartedFlight_) {
        hasStartedFlight_ = true;
        std::string entrySide = "right";
        if (isEnteringFromLeft_) { entrySide = "left"; }
        Logger::Log("[LoopEnemy] Flight started: entrySide=" + entrySide +
            ", delaySeconds=" + std::to_string(spawnDelaySeconds_));
    }
    if (elapsedSeconds_ >= LoopEnemyPath::kDurationSeconds) {
        Logger::Log("[LoopEnemy] Flight completed: one loop and exit");
        hasEscaped_ = true;
        SetDead(true);
        return;
    }
    NormalEnemy::Update();
}

void LoopEnemy::Move()
{
    const LoopEnemyPathSample sample = LoopEnemyPath::Sample(elapsedSeconds_, isEnteringFromLeft_);
    transform_.translate = railOrigin_ + railRight_ * (centerOffset_.x + sample.position.x) +
        railUp_ * (centerOffset_.y + sample.position.y) + railForward_ * centerOffset_.z;
    const Vector3 direction = Normalize(railRight_ * sample.velocity.x + railUp_ * sample.velocity.y);
    const float horizontalLength = std::sqrt(direction.x * direction.x + direction.z * direction.z);
    transform_.rotate.x = -std::atan2(direction.y, horizontalLength);
    transform_.rotate.y = -std::atan2(direction.x, direction.z);
    transform_.rotate.z = 0.0f;
}

void LoopEnemy::Draw()
{
    if (hasStartedFlight_ && !hasEscaped_) { BaseEnemy::Draw(); }
}

void LoopEnemy::DrawShadow(ShadowMapRenderer& renderer)
{
    if (hasStartedFlight_ && !hasEscaped_) { BaseEnemy::DrawShadow(renderer); }
}

void LoopEnemy::SubmitRaytracing(DxrRenderer& renderer)
{
    if (hasStartedFlight_ && !hasEscaped_) { BaseEnemy::SubmitRaytracing(renderer); }
}

void LoopEnemy::GetCollisionParts(std::vector<EnemyCollisionPart>& parts) const
{
    if (hasStartedFlight_ && !IsDead() && !hasEscaped_) { BaseEnemy::GetCollisionParts(parts); }
}

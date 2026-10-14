#include "App/Game/Enemy/SwarmEnemy/SwarmEnemy.h"

#include "App/Game/Enemy/Bullet/SwarmPulseBullet.h"
#include "App/Game/Player/Player.h"
#include "Engine/Effect/EffectManager.h"
#include "Engine/math/MathStruct.h"
#include "Engine/Time/TimeManager.h"
#include <cmath>
#include <numbers>

namespace {
constexpr float kSwarmExitX = 40.0f;
constexpr float kSwarmBulletSpeed = 1.15f;
constexpr float kFadeOutDuration = 0.70f;
constexpr float kAttackIntervalBase = 0.90f;
constexpr int32_t kMaximumShots = 2;
}

void SwarmEnemy::Initialize(
    Model* model,
    Model* bulletModel,
    Player* player,
    const std::shared_ptr<SwarmGroupState>& groupState,
    SwarmFormationType formationType,
    int32_t slotIndex,
    int32_t travelDirection)
{
    BaseEnemy::Initialize(model);

    bulletModel_ = bulletModel;
    player_ = player;
    groupState_ = groupState;
    const auto& formation = GetSwarmFormation(formationType);
    formation_ = formation.strategy;
    slotIndex_ = slotIndex;
    travelDirection_ = travelDirection;
    if (player_ != nullptr) {
        previousPlayerPositionZ_ = player_->GetTranslate().z;
        forwardPositionZ_ = previousPlayerPositionZ_ + forwardDistance_;
    }
    SetHealth(1.0f);
    transform_.scale = { 0.58f, 0.58f, 0.58f };
    attackTimer_ =
        0.35f +
        static_cast<float>(slotIndex_ % 6) * 0.13f;

    centerStartX_ = -38.0f;
    if (travelDirection_ < 0) {
        centerStartX_ = 38.0f;
        transform_.rotate.y = std::numbers::pi_v<float>;
    }

    if (object_ != nullptr) {
        object_->SetScale(transform_.scale);
        object_->SetEnableLighting(false);

        baseColor_ = formation.color;
        object_->SetColor(baseColor_);
    }

    Move();
    if (object_ != nullptr) {
        object_->SetScale(transform_.scale);
        object_->SetRotate(transform_.rotate);
        object_->SetTranslate(transform_.translate);
        object_->Update();
    }
}

void SwarmEnemy::Update()
{
    BaseEnemy::Update();
}

void SwarmEnemy::GetCollisionParts(std::vector<EnemyCollisionPart>& parts) const
{
    if (isFading_) {
        return;
    }

    EnemyCollisionPart part {};
    part.position = transform_.translate;
    part.radius = 1.15f;
    part.partIndex = 0;
    parts.push_back(part);
}

void SwarmEnemy::Move()
{
    if (player_ == nullptr) {
        return;
    }

    if (isFading_) {
        fadeTimer_ += TimeManager::GetInstance()->GetDeltaTime();
        float fadeRatio = 1.0f - fadeTimer_ / kFadeOutDuration;
        if (fadeRatio < 0.0f) {
            fadeRatio = 0.0f;
        }

        if (object_ != nullptr) {
            Vector4 fadeColor = baseColor_;
            fadeColor.w = fadeRatio;
            object_->SetColor(fadeColor);
        }

        if (fadeTimer_ >= kFadeOutDuration) {
            escaped_ = true;
            SetDead(true);
        }
        return;
    }

    moveTime_ += TimeManager::GetInstance()->GetDeltaTime();

    float centerX = centerStartX_;
    centerX += static_cast<float>(travelDirection_) * crossingSpeed_ * moveTime_;

    Vector3 formationOffset = CalculateFormationOffset();
    // 敵はプレイヤー更新前に動くため、直前の移動に使った倍率を参照する。
    // 通常の進行分だけ追従し、ブーストで縮めた距離は解除後も維持する。
    const float playerPositionZ = player_->GetTranslate().z;
    const float playerMovementZ = playerPositionZ - previousPlayerPositionZ_;
    forwardPositionZ_ += playerMovementZ / player_->GetForwardSpeedMultiplier();
    previousPlayerPositionZ_ = playerPositionZ;
    transform_.translate.x = centerX + formationOffset.x;
    transform_.translate.y = baseHeight_ + formationOffset.y;
    transform_.translate.z =
        forwardPositionZ_ + formationOffset.z;
    transform_.rotate.z =
        std::sin(moveTime_ * 4.0f + static_cast<float>(slotIndex_) * 0.25f) * 0.28f;

    bool passedExit = false;
    if (travelDirection_ > 0 && transform_.translate.x > kSwarmExitX) {
        passedExit = true;
    }
    if (travelDirection_ < 0 && transform_.translate.x < -kSwarmExitX) {
        passedExit = true;
    }

    if (passedExit) {
        isFading_ = true;
        fadeTimer_ = 0.0f;
    }
}

void SwarmEnemy::SetAppearance(Model* model)
{
    BaseEnemy::SetAppearance(model);
    // フェードアウト中も、隊形の色で氷のテクスチャを上書きしない。
    baseColor_ = { 1.0f, 1.0f, 1.0f, 1.0f };
}

void SwarmEnemy::Attack()
{
    if (IsDead()) {
        return;
    }
    if (isFading_) {
        return;
    }
    if (player_ == nullptr || bulletModel_ == nullptr) {
        return;
    }
    if (shotsFired_ >= kMaximumShots) {
        return;
    }

    attackTimer_ -= TimeManager::GetInstance()->GetDeltaTime();
    if (attackTimer_ > 0.0f) {
        return;
    }

    FireMovingBullet();
    shotsFired_ += 1;
    attackTimer_ =
        kAttackIntervalBase +
        static_cast<float>(slotIndex_ % 4) * 0.12f;
}

void SwarmEnemy::OnDeath()
{
    if (!groupState_) {
        return;
    }

    groupState_->RecordRemoval(escaped_);
    if (escaped_) { return; }

    EffectManager::GetInstance()->PlayEffect("HitEffect", transform_.translate);
}

void SwarmEnemy::FireMovingBullet()
{
    if (player_ == nullptr) {
        return;
    }
    if (bulletModel_ == nullptr) {
        return;
    }

    Vector3 targetPosition = player_->GetTranslate();

    // 群れ全体が正確になりすぎないよう、通常弾より控えめに45%先読みする。
    constexpr float kPredictionRatio = 0.45f;
    const float distance = Vector3Length(
        targetPosition - transform_.translate);
    if (kSwarmBulletSpeed > 0.0f) {
        const float flightFrames = distance / kSwarmBulletSpeed;
        targetPosition += player_->GetAutomaticWorldVelocity() *
            (flightFrames * kPredictionRatio);
    }

    Vector3 direction = Normalize(targetPosition - transform_.translate);
    Vector3 velocity = direction * kSwarmBulletSpeed;

    std::unique_ptr<SwarmPulseBullet> bullet =
        std::make_unique<SwarmPulseBullet>();
    bullet->Initialize(bulletModel_);
    bullet->SetTranslate(transform_.translate);
    bullet->SetSwarmVelocity(velocity);
    bullet->SetWavePhase(static_cast<float>(slotIndex_) * 0.72f);

    AddEnemyBullet(std::move(bullet));
    EffectManager::GetInstance()->PlayEffect("WormShotFlash", transform_.translate);
}

Vector3 SwarmEnemy::CalculateFormationOffset() const
{
    const SwarmFormationContext context { slotIndex_, travelDirection_, moveTime_ };
    return formation_->CalculateOffset(context);
}

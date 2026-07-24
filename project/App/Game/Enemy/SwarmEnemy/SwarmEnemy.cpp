#include "App/Game/Enemy/SwarmEnemy/SwarmEnemy.h"

#include "App/Game/Enemy/Bullet/SwarmPulseBullet.h"
#include "App/Game/Player/Player.h"
#include "Engine/Effect/EffectManager.h"
#include "Engine/math/MathStruct.h"
#include <cmath>
#include <numbers>

namespace {
constexpr int32_t kSwarmMemberCount = 18;
constexpr float kSwarmExitX = 40.0f;
constexpr float kSwarmBulletSpeed = 1.15f;
constexpr float kVolleyHoldDuration = 1.10f;
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
    formationType_ = formationType;
    slotIndex_ = slotIndex;
    travelDirection_ = travelDirection;
    hp_ = 1.0f;
    transform_.scale = { 0.58f, 0.58f, 0.58f };

    centerStartX_ = -38.0f;
    if (travelDirection_ < 0) {
        centerStartX_ = 38.0f;
        transform_.rotate.y = std::numbers::pi_v<float>;
    }

    if (object_ != nullptr) {
        object_->SetScale(transform_.scale);
        object_->SetEnableLighting(false);

        if (slotIndex_ % 2 == 0) {
            object_->SetColor({ 0.20f, 0.95f, 1.0f, 1.0f });
        } else {
            object_->SetColor({ 1.0f, 0.25f, 0.82f, 1.0f });
        }
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

    constexpr float kDeltaTime = 1.0f / 60.0f;

    if (groupState_ &&
        groupState_->volleyTriggered &&
        volleyHoldTimer_ < kVolleyHoldDuration) {
        volleyHoldTimer_ += kDeltaTime;
        Vector3 formationOffset = CalculateFormationOffset();
        transform_.translate.y = baseHeight_ + formationOffset.y;
        transform_.translate.z =
            player_->GetTranslate().z + forwardDistance_ + formationOffset.z;
        return;
    }

    moveTime_ += kDeltaTime;

    float centerX = centerStartX_;
    centerX += static_cast<float>(travelDirection_) * crossingSpeed_ * moveTime_;

    Vector3 formationOffset = CalculateFormationOffset();
    transform_.translate.x = centerX + formationOffset.x;
    transform_.translate.y = baseHeight_ + formationOffset.y;
    transform_.translate.z =
        player_->GetTranslate().z + forwardDistance_ + formationOffset.z;
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
        escaped_ = true;
        SetDead(true);
    }
}

void SwarmEnemy::Attack()
{
    if (isDead_) {
        return;
    }
    if (!groupState_) {
        return;
    }
    if (!groupState_->volleyTriggered) {
        return;
    }
    if (firedVolley_) {
        return;
    }

    firedVolley_ = true;
    FireVolleyBullet();
}

void SwarmEnemy::OnDeath()
{
    if (!groupState_) {
        return;
    }

    groupState_->activeCount -= 1;
    if (groupState_->activeCount < 0) {
        groupState_->activeCount = 0;
    }

    if (escaped_) {
        groupState_->escapedCount += 1;
        if (!groupState_->volleyTriggered &&
            groupState_->escapedCount >= groupState_->escapeThreshold) {
            groupState_->volleyTriggered = true;
        }
        return;
    }

    groupState_->defeatedCount += 1;
    EffectManager::GetInstance()->PlayEffect("HitEffect", transform_.translate);
}

void SwarmEnemy::FireVolleyBullet()
{
    if (player_ == nullptr) {
        return;
    }
    if (bulletModel_ == nullptr) {
        return;
    }

    Vector3 direction = Normalize(player_->GetTranslate() - transform_.translate);
    Vector3 velocity = direction * kSwarmBulletSpeed;

    std::unique_ptr<SwarmPulseBullet> bullet =
        std::make_unique<SwarmPulseBullet>();
    bullet->Initialize(bulletModel_);
    bullet->SetTranslate(transform_.translate);
    bullet->SetSwarmVelocity(velocity);
    bullet->SetWavePhase(static_cast<float>(slotIndex_) * 0.72f);

    enemyBullets_.push_back(std::move(bullet));
    EffectManager::GetInstance()->PlayEffect("ShotBullet", transform_.translate);
}

Vector3 SwarmEnemy::CalculateFormationOffset() const
{
    if (formationType_ == SwarmFormationType::Wall) {
        return CalculateWallOffset();
    }
    if (formationType_ == SwarmFormationType::Glyph) {
        return CalculateGlyphOffset();
    }
    return CalculateSpiralOffset();
}

Vector3 SwarmEnemy::CalculateSpiralOffset() const
{
    float slotRate =
        static_cast<float>(slotIndex_) / static_cast<float>(kSwarmMemberCount);
    float angle = slotRate * std::numbers::pi_v<float> * 2.0f;
    angle += moveTime_ * 2.4f;

    Vector3 offset {};
    offset.x = std::cos(angle) * 4.5f;
    offset.y = std::sin(angle) * 8.0f;
    offset.z = std::sin(angle * 2.0f) * 3.0f;
    return offset;
}

Vector3 SwarmEnemy::CalculateWallOffset() const
{
    constexpr int32_t kColumnCount = 6;
    int32_t column = slotIndex_ % kColumnCount;
    int32_t row = slotIndex_ / kColumnCount;

    Vector3 offset {};
    offset.x = (static_cast<float>(column) - 2.5f) * 2.5f;
    offset.y = (static_cast<float>(row) - 1.0f) * 6.0f;
    offset.z =
        std::sin(moveTime_ * 3.0f + static_cast<float>(column) * 0.5f) * 1.2f;
    return offset;
}

Vector3 SwarmEnemy::CalculateGlyphOffset() const
{
    constexpr int32_t kLeftBranchCount = 9;
    Vector3 offset {};
    float branchRate = 0.0f;

    if (slotIndex_ < kLeftBranchCount) {
        branchRate =
            static_cast<float>(slotIndex_) /
            static_cast<float>(kLeftBranchCount - 1);
        offset.x = -9.0f + branchRate * 9.0f;
        offset.y = 8.0f - branchRate * 14.0f;
    } else {
        int32_t rightIndex = slotIndex_ - kLeftBranchCount;
        branchRate =
            static_cast<float>(rightIndex) /
            static_cast<float>(kSwarmMemberCount - kLeftBranchCount - 1);
        offset.x = branchRate * 9.0f;
        offset.y = -6.0f + branchRate * 14.0f;
    }

    offset.y +=
        std::sin(moveTime_ * 3.2f + static_cast<float>(slotIndex_) * 0.35f) *
        0.45f;
    offset.z =
        std::cos(moveTime_ * 2.0f + static_cast<float>(slotIndex_) * 0.22f) *
        1.3f;
    return offset;
}

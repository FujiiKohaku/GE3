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
constexpr float kFadeOutDuration = 0.70f;
constexpr float kAttackIntervalBase = 0.90f;
constexpr int32_t kMaximumShots = 2;
constexpr float kDeltaTime = 1.0f / 60.0f;
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

        baseColor_ = { 0.20f, 0.95f, 1.0f, 1.0f };
        if (formationType_ == SwarmFormationType::Glyph ||
            formationType_ == SwarmFormationType::Arrow) {
            baseColor_ = { 1.0f, 0.25f, 0.82f, 1.0f };
        }
        if (formationType_ == SwarmFormationType::Diamond) {
            baseColor_ = { 1.0f, 0.82f, 0.18f, 1.0f };
        }
        if (formationType_ == SwarmFormationType::Wave) {
            baseColor_ = { 0.35f, 1.0f, 0.30f, 1.0f };
        }
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
        fadeTimer_ += kDeltaTime;
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
        isFading_ = true;
        fadeTimer_ = 0.0f;
    }
}

void SwarmEnemy::Attack()
{
    if (isDead_) {
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

    attackTimer_ -= kDeltaTime;
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

    groupState_->activeCount -= 1;
    if (groupState_->activeCount < 0) {
        groupState_->activeCount = 0;
    }

    if (escaped_) {
        groupState_->escapedCount += 1;
        return;
    }

    groupState_->defeatedCount += 1;
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
    if (formationType_ == SwarmFormationType::Diamond) {
        return CalculateDiamondOffset();
    }
    if (formationType_ == SwarmFormationType::Wave) {
        return CalculateWaveOffset();
    }
    if (formationType_ == SwarmFormationType::Arrow) {
        return CalculateArrowOffset();
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

Vector3 SwarmEnemy::CalculateDiamondOffset() const
{
    int32_t row = 0;
    int32_t indexInRow = slotIndex_;
    int32_t rowCount = 2;

    if (slotIndex_ >= 2 && slotIndex_ < 6) {
        row = 1;
        indexInRow = slotIndex_ - 2;
        rowCount = 4;
    }
    if (slotIndex_ >= 6 && slotIndex_ < 12) {
        row = 2;
        indexInRow = slotIndex_ - 6;
        rowCount = 6;
    }
    if (slotIndex_ >= 12 && slotIndex_ < 16) {
        row = 3;
        indexInRow = slotIndex_ - 12;
        rowCount = 4;
    }
    if (slotIndex_ >= 16) {
        row = 4;
        indexInRow = slotIndex_ - 16;
        rowCount = 2;
    }

    Vector3 offset {};
    offset.x =
        (static_cast<float>(indexInRow) -
         static_cast<float>(rowCount - 1) * 0.5f) *
        2.6f;
    offset.y = 8.0f - static_cast<float>(row) * 4.0f;
    offset.z =
        std::sin(moveTime_ * 2.4f + static_cast<float>(row) * 0.6f) *
        0.8f;
    return offset;
}

Vector3 SwarmEnemy::CalculateWaveOffset() const
{
    float slotCenter =
        static_cast<float>(slotIndex_) -
        static_cast<float>(kSwarmMemberCount - 1) * 0.5f;
    float wavePhase =
        static_cast<float>(slotIndex_) * 0.72f +
        moveTime_ * 2.8f;

    Vector3 offset {};
    offset.x = slotCenter * 1.45f;
    offset.y = std::sin(wavePhase) * 6.5f;
    offset.z = std::cos(wavePhase) * 2.2f;
    return offset;
}

Vector3 SwarmEnemy::CalculateArrowOffset() const
{
    constexpr int32_t kWingCount = 9;
    int32_t wingIndex = slotIndex_;
    float verticalDirection = 1.0f;

    if (slotIndex_ >= kWingCount) {
        wingIndex = slotIndex_ - kWingCount;
        verticalDirection = -1.0f;
    }

    float wingRate =
        static_cast<float>(wingIndex) /
        static_cast<float>(kWingCount - 1);

    Vector3 offset {};
    offset.x =
        static_cast<float>(travelDirection_) *
        (-10.0f + wingRate * 13.0f);
    offset.y = verticalDirection * (8.0f - wingRate * 8.0f);
    offset.y +=
        std::sin(moveTime_ * 3.0f + static_cast<float>(slotIndex_) * 0.3f) *
        0.35f;
    offset.z =
        std::cos(moveTime_ * 2.2f + static_cast<float>(slotIndex_) * 0.2f) *
        1.1f;
    return offset;
}

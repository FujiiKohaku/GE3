#include "App/Game/Boss/FearWormEnemy/FearWormEnemy.h"

#include "App/Game/Enemy/Bullet/NormalEnemyBullet.h"
#include "App/Game/Player/Player.h"
#include "Engine/3D/Object3d.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/Effect/EffectManager.h"

#include <cmath>
#include <utility>

namespace {
constexpr int32_t kWormSegmentCount = 10;
constexpr float kFrameTime = 1.0f / 60.0f;
constexpr float kHeadHp = 20.0f;
constexpr float kBodyHp = 6.0f;
constexpr float kAttackRange = 240.0f;
constexpr float kHeadChargeDuration = 0.85f;
constexpr float kHeadChargeShotInterval = 0.22f;
constexpr float kHeadChargeCooldown = 3.10f;
constexpr float kHeadChargeEffectInterval = 0.14f;
constexpr float kChargedBulletSpeed = 1.55f;
constexpr float kChargedBulletScale = 0.88f;
constexpr float kChargedBulletCollisionRadius = 2.75f;
constexpr int32_t kHeadChargeShotMax = 3;
constexpr int32_t kChargedBulletDamage = 3;
constexpr float kDeathExplosionInterval = 0.10f;
constexpr float kDeathSequenceEndDelay = 0.80f;

}

void FearWormEnemy::Initialize(
    Model* model,
    Model* bulletModel,
    Player* player)
{
    bulletModel_ = bulletModel;
    player_ = player;
    hp_ = kHeadHp;
    moveSpeed_ = 0.10f;

    InitializeSegments(model);
    SetPosition(transform_.translate);
}

void FearWormEnemy::InitializeSegments(Model* model)
{
    segments_.clear();
    segments_.reserve(kWormSegmentCount);

    for (int32_t index = 0; index < kWormSegmentCount; index = index + 1) {
        Segment segment {};
        segment.object = std::make_unique<Object3d>();
        segment.object->Initialize(Object3dManager::GetInstance());
        segment.object->SetModel(model);
        segment.object->SetEnableLighting(false);
        segment.isHead = false;
        segment.isAlive = true;
        segment.hp = kBodyHp;

        float scale = 2.45f;
        if (index == 0) {
            segment.isHead = true;
            segment.hp = kHeadHp;
            scale = 3.45f;
        }

        if (index == kWormSegmentCount - 1) {
            scale = 2.15f;
        }

        segment.scale = { scale, scale, scale };
        segment.radius = scale * 0.90f;

        segments_.push_back(std::move(segment));
    }
}

void FearWormEnemy::SetPosition(const Vector3& position)
{
    transform_.translate = position;
    startPosition_ = position;
    isParallelStarted_ = false;
    movementPattern_ = MovementPattern::Orbit;
    movementPatternTimer_ = 0.0f;
    isHeadChargeActive_ = false;
    isHeadChargeFiring_ = false;
    headChargeTimer_ = 0.0f;
    headChargeShotTimer_ = 0.0f;
    headChargeCooldownTimer_ = 1.8f;
    headChargeEffectTimer_ = 0.0f;
    headChargeShotCount_ = 0;
    headAimDirection_ = { 0.0f, 0.0f, -1.0f };
    headTrail_.clear();

    for (size_t index = 0; index < segments_.size(); index = index + 1) {
        float offset = segmentSpacing_ * static_cast<float>(index);
        segments_[index].position = position;
        segments_[index].position.z -= offset;
    }

    ResetHeadTrail();
}

Vector3 FearWormEnemy::GetPosition() const
{
    if (segments_.empty()) {
        return transform_.translate;
    }

    return segments_[0].position;
}

void FearWormEnemy::Update()
{
    if (isDead_) {
        UpdateDeathSequence();
        UpdateBullets();
        RemoveDeadBullets();
        return;
    }

    moveTime_ += kFrameTime;

    UpdateMovement();
    UpdateSegments();
    UpdateSegmentObjects();
    Attack();
    UpdateBullets();
    RemoveDeadBullets();

    if (!vulnerableEffectPlayed_ && !HasAliveBodyParts()) {
        vulnerableEffectPlayed_ = true;
        PlayHeadVulnerableEffect(GetPosition());
    }
}

void FearWormEnemy::UpdateMovement()
{
    if (segments_.empty()) {
        return;
    }

    Vector3 target = startPosition_;
    float movementSpeedRate =
        CalculateMovementSpeedRate();

    if (player_ != nullptr) {
        Vector3 playerPosition = player_->GetTranslate();

        if (!isParallelStarted_) {
            if (playerPosition.z >= startPosition_.z - activationLeadDistance_) {
                StartOrbitEntry(playerPosition);
            }
        }

        if (isParallelStarted_) {
            orbitAngle_ +=
                kFrameTime *
                1.15f *
                movementSpeedRate;

            if (enterTimer_ < enterDuration_) {
                enterTimer_ += kFrameTime;

                float enterRate =
                    enterTimer_ /
                    enterDuration_;

                if (enterRate > 1.0f) {
                    enterRate = 1.0f;
                }

                target = Lerp(
                    CalculateEntryStartPosition(playerPosition),
                    CalculateMovementTargetPosition(playerPosition),
                    enterRate);
            } else {
                UpdateMovementPattern(movementSpeedRate);
                target = CalculateMovementTargetPosition(playerPosition);
            }
        } else {
            target = CalculateEntryStartPosition(playerPosition);
        }
    } else {
        target.x += std::sin(moveTime_ * 1.00f) * 6.0f;
        target.y += std::sin(moveTime_ * 1.30f) * 2.5f;
        target.z += std::sin(moveTime_ * 0.60f) * 3.0f;
    }

    segments_[0].position = Lerp(
        segments_[0].position,
        target,
        moveSpeed_ * movementSpeedRate);

    transform_.translate = segments_[0].position;

    RecordHeadTrail();
}

Vector3 FearWormEnemy::CalculateEntryStartPosition(const Vector3& playerPosition) const
{
    Vector3 result = playerPosition;
    result.x -= 34.0f;
    result.y += 18.0f;
    result.z += parallelForwardOffset_ + 22.0f;
    return result;
}

Vector3 FearWormEnemy::CalculateOrbitTargetPosition(const Vector3& playerPosition) const
{
    Vector3 result = playerPosition;

    float orbitX =
        std::cos(orbitAngle_) *
        16.0f;

    float orbitY =
        std::sin(orbitAngle_) *
        7.0f;

    float lateralSway =
        std::sin(orbitAngle_ * 2.0f) *
        2.0f;

    float depthSway =
        std::sin(orbitAngle_ * 0.80f) *
        3.0f;

    result.x += orbitX + lateralSway;
    result.y += 7.0f + orbitY;
    result.z += parallelForwardOffset_ + depthSway;

    return result;
}

Vector3 FearWormEnemy::CalculateCoilTargetPosition(const Vector3& playerPosition) const
{
    Vector3 result = playerPosition;

    float coilAngle =
        orbitAngle_ *
        2.35f;

    float radius =
        8.0f +
        std::sin(movementPatternTimer_ * 3.2f) *
        2.0f;

    result.x += std::cos(coilAngle) * radius;
    result.y += 8.0f + std::sin(coilAngle) * radius * 0.75f;
    result.z += parallelForwardOffset_ + std::sin(coilAngle * 0.50f) * 2.5f;

    return result;
}

Vector3 FearWormEnemy::CalculateWeaveTargetPosition(const Vector3& playerPosition) const
{
    Vector3 result = playerPosition;

    float waveAngle =
        orbitAngle_ *
        1.35f;

    result.x += std::sin(waveAngle) * 13.0f;
    result.y += 7.0f + std::sin(waveAngle * 2.0f) * 9.0f;
    result.z += parallelForwardOffset_ + std::cos(waveAngle) * 2.5f;

    return result;
}

Vector3 FearWormEnemy::CalculateDriftTargetPosition(const Vector3& playerPosition) const
{
    Vector3 result = playerPosition;

    result.x += std::sin(orbitAngle_ * 0.65f) * 8.0f;
    result.y += 8.0f + std::sin(orbitAngle_ * 0.85f) * 4.5f;
    result.z += parallelForwardOffset_ + std::cos(orbitAngle_ * 0.50f) * 2.0f;

    return result;
}

Vector3 FearWormEnemy::CalculateMovementTargetPosition(const Vector3& playerPosition) const
{
    if (movementPattern_ == MovementPattern::Coil) {
        return CalculateCoilTargetPosition(playerPosition);
    }

    if (movementPattern_ == MovementPattern::Weave) {
        return CalculateWeaveTargetPosition(playerPosition);
    }

    if (movementPattern_ == MovementPattern::Drift) {
        return CalculateDriftTargetPosition(playerPosition);
    }

    return CalculateOrbitTargetPosition(playerPosition);
}

void FearWormEnemy::UpdateMovementPattern(float movementSpeedRate)
{
    movementPatternTimer_ +=
        kFrameTime *
        movementSpeedRate;

    if (movementPatternTimer_ < movementPatternDuration_) {
        return;
    }

    movementPatternTimer_ = 0.0f;

    if (movementPattern_ == MovementPattern::Orbit) {
        movementPattern_ = MovementPattern::Coil;
        return;
    }

    if (movementPattern_ == MovementPattern::Coil) {
        movementPattern_ = MovementPattern::Weave;
        return;
    }

    if (movementPattern_ == MovementPattern::Weave) {
        movementPattern_ = MovementPattern::Drift;
        return;
    }

    movementPattern_ = MovementPattern::Orbit;
}

void FearWormEnemy::StartOrbitEntry(const Vector3& playerPosition)
{
    isParallelStarted_ = true;
    enterTimer_ = 0.0f;
    orbitAngle_ = 2.35f;
    movementPattern_ = MovementPattern::Orbit;
    movementPatternTimer_ = 0.0f;

    Vector3 entryPosition =
        CalculateEntryStartPosition(playerPosition);

    segments_[0].position = entryPosition;
    transform_.translate = entryPosition;

    ResetHeadTrail();
}

void FearWormEnemy::UpdateSegments()
{
    int32_t aliveBodyOrder = 1;

    for (size_t index = 1; index < segments_.size(); index = index + 1) {
        Segment& segment = segments_[index];

        if (!segment.isAlive) {
            continue;
        }

        float distanceFromHead =
            segmentSpacing_ *
            static_cast<float>(aliveBodyOrder);

        Vector3 targetPosition =
            SampleHeadTrail(distanceFromHead);

        if (aliveBodyOrder == static_cast<int32_t>(index)) {
            segment.position = targetPosition;
        } else {
            segment.position = Lerp(
                segment.position,
                targetPosition,
                0.35f);
        }

        aliveBodyOrder = aliveBodyOrder + 1;
    }
}

void FearWormEnemy::ResetHeadTrail()
{
    headTrail_.clear();

    if (segments_.empty()) {
        return;
    }

    Vector3 trailPosition =
        segments_[0].position;

    const int32_t trailCount = 80;
    for (int32_t index = 0; index < trailCount; index = index + 1) {
        headTrail_.push_back(trailPosition);
        trailPosition.z -= headTrailSampleStep_;
    }
}

void FearWormEnemy::RecordHeadTrail()
{
    if (segments_.empty()) {
        return;
    }

    Vector3 headPosition =
        segments_[0].position;

    if (!headTrail_.empty()) {
        Vector3 difference =
            headPosition -
            headTrail_[0];

        if (Vector3Length(difference) < headTrailSampleStep_) {
            return;
        }
    }

    headTrail_.insert(
        headTrail_.begin(),
        headPosition);

    const size_t maxTrailCount = 120;
    if (headTrail_.size() > maxTrailCount) {
        headTrail_.erase(
            headTrail_.begin() + maxTrailCount,
            headTrail_.end());
    }
}

Vector3 FearWormEnemy::SampleHeadTrail(float distanceFromHead) const
{
    if (headTrail_.empty()) {
        return GetPosition();
    }

    if (headTrail_.size() == 1) {
        return headTrail_[0];
    }

    float accumulatedDistance = 0.0f;

    for (size_t index = 1; index < headTrail_.size(); index = index + 1) {
        Vector3 previous =
            headTrail_[index - 1];

        Vector3 current =
            headTrail_[index];

        float segmentDistance = Vector3Length(current - previous);

        if (segmentDistance <= 0.0001f) {
            continue;
        }

        if (accumulatedDistance + segmentDistance >= distanceFromHead) {
            float remainingDistance =
                distanceFromHead -
                accumulatedDistance;

            float rate =
                remainingDistance /
                segmentDistance;

            return Lerp(
                previous,
                current,
                rate);
        }

        accumulatedDistance += segmentDistance;
    }

    return headTrail_[headTrail_.size() - 1];
}

void FearWormEnemy::UpdateSegmentObjects()
{
    for (size_t index = 0; index < segments_.size(); index = index + 1) {
        Segment& segment = segments_[index];

        if (segment.object == nullptr) {
            continue;
        }

        if (segment.hitFlashTimer > 0.0f) {
            segment.hitFlashTimer -= kFrameTime;
            if (segment.hitFlashTimer < 0.0f) {
                segment.hitFlashTimer = 0.0f;
            }
        }

        Vector4 color {};
        if (segment.isHead) {
            color = { 0.30f, 0.88f, 1.0f, 1.0f };

            if (!HasAliveBodyParts()) {
                color = { 1.0f, 0.18f, 0.04f, 1.0f };
            }

            if (isHeadChargeActive_) {
                color = { 1.0f, 0.48f, 0.08f, 1.0f };
            }

            if (isHeadChargeFiring_) {
                color = { 1.0f, 0.08f, 0.02f, 1.0f };
            }
        } else {
            color = { 0.78f, 0.20f, 1.0f, 1.0f };
        }

        if (segment.hitFlashTimer > 0.0f) {
            color = { 1.0f, 0.95f, 0.35f, 1.0f };
        }

        float pulse = 1.0f + std::sin(moveTime_ * 5.5f + static_cast<float>(index)) * 0.08f;
        if (segment.isHead && isHeadChargeActive_) {
            pulse += 0.12f;
            pulse += std::sin(headChargeTimer_ * 24.0f) * 0.08f;
        }

        Vector3 scale {};
        scale.x = segment.scale.x * pulse;
        scale.y = segment.scale.y * pulse;
        scale.z = segment.scale.z * pulse;

        Vector3 rotate {};
        rotate.y = moveTime_ + static_cast<float>(index) * 0.35f;
        if (segment.isHead && isHeadChargeActive_) {
            rotate = CalculateHeadLookRotation();
        }

        segment.object->SetScale(scale);
        segment.object->SetRotate(rotate);
        segment.object->SetTranslate(segment.position);
        segment.object->SetColor(color);
        segment.object->Update();
    }
}

void FearWormEnemy::Draw()
{
    if (isParallelStarted_) {
        for (Segment& segment : segments_) {
            if (!segment.isAlive) {
                continue;
            }

            if (segment.object == nullptr) {
                continue;
            }

            segment.object->Draw();
        }
    }

    for (std::unique_ptr<EnemyBullet>& bullet : enemyBullets_) {
        bullet->Draw();
    }
}

bool FearWormEnemy::IsDeathSequenceFinished() const
{
    return isDeathSequenceFinished_;
}

void FearWormEnemy::GetCollisionParts(std::vector<EnemyCollisionPart>& parts) const
{
    if (!isParallelStarted_) {
        return;
    }

    for (size_t index = 0; index < segments_.size(); index = index + 1) {
        const Segment& segment = segments_[index];

        if (!segment.isAlive) {
            continue;
        }

        EnemyCollisionPart part {};
        part.position = segment.position;
        part.radius = segment.radius;
        part.partIndex = static_cast<int32_t>(index);

        parts.push_back(part);
    }
}

bool FearWormEnemy::IsCollisionPartDamageable(int32_t partIndex) const
{
    if (!IsValidSegmentIndex(partIndex)) {
        return false;
    }

    const Segment& segment = segments_[partIndex];
    if (!segment.isAlive) {
        return false;
    }

    if (segment.isHead && HasAliveBodyParts()) {
        return false;
    }

    return true;
}

void FearWormEnemy::ApplyDamageToPart(int32_t partIndex, float damage)
{
    if (!IsValidSegmentIndex(partIndex)) {
        return;
    }

    if (!IsCollisionPartDamageable(partIndex)) {
        OnCollisionPartGuarded(partIndex, segments_[partIndex].position);
        return;
    }

    Segment& segment = segments_[partIndex];
    segment.hp -= damage;
    segment.hitFlashTimer = 0.10f;

    if (segment.hp > 0.0f) {
        return;
    }

    if (segment.isHead) {
        SetDead(true);
        return;
    }

    segment.isAlive = false;
    PlayBodyBreakEffect(segment.position);

    if (!vulnerableEffectPlayed_ && !HasAliveBodyParts()) {
        vulnerableEffectPlayed_ = true;
        PlayHeadVulnerableEffect(GetPosition());
    }
}

void FearWormEnemy::OnCollisionPartGuarded(int32_t partIndex, const Vector3& position)
{
    if (!IsValidSegmentIndex(partIndex)) {
        return;
    }

    segments_[partIndex].hitFlashTimer = 0.08f;
    PlayHeadGuardEffect(position);
}

void FearWormEnemy::Attack()
{
    if (!isParallelStarted_) {
        return;
    }

    if (player_ == nullptr) {
        return;
    }

    if (bulletModel_ == nullptr) {
        return;
    }

    Vector3 toPlayer = player_->GetTranslate() - GetPosition();
    float distance = Vector3Length(toPlayer);

    if (distance > kAttackRange) {
        if (isHeadChargeActive_) {
            FinishHeadChargeAttack();
        }
        return;
    }

    UpdateHeadAimDirection();
    UpdateHeadChargeAttack(CalculateMovementSpeedRate());

    if (isHeadChargeActive_) {
        return;
    }

    fireTimer_ = fireTimer_ + 1;
    if (fireTimer_ < fireInterval_) {
        return;
    }

    fireTimer_ = 0;

    int32_t segmentCount = static_cast<int32_t>(segments_.size());
    for (int32_t count = 0; count < segmentCount; count = count + 1) {
        fireSegmentIndex_ = fireSegmentIndex_ + 1;

        if (fireSegmentIndex_ >= segmentCount) {
            fireSegmentIndex_ = 0;
        }

        if (!segments_[fireSegmentIndex_].isAlive) {
            continue;
        }

        FireBullet(segments_[fireSegmentIndex_].position);
        break;
    }
}

void FearWormEnemy::UpdateHeadChargeAttack(float attackSpeedRate)
{
    float deltaTime =
        kFrameTime *
        attackSpeedRate;

    if (!isHeadChargeActive_) {
        if (headChargeCooldownTimer_ > 0.0f) {
            headChargeCooldownTimer_ -= deltaTime;
            if (headChargeCooldownTimer_ < 0.0f) {
                headChargeCooldownTimer_ = 0.0f;
            }
            return;
        }

        StartHeadChargeAttack();
        return;
    }

    UpdateHeadAimDirection();

    if (!isHeadChargeFiring_) {
        headChargeTimer_ += deltaTime;
        headChargeEffectTimer_ += deltaTime;

        if (headChargeEffectTimer_ >= kHeadChargeEffectInterval) {
            headChargeEffectTimer_ = 0.0f;
            EffectManager::GetInstance()->PlayEffect(
                "WormShotFlash",
                CalculateHeadMuzzlePosition());
        }

        if (headChargeTimer_ < kHeadChargeDuration) {
            return;
        }

        isHeadChargeFiring_ = true;
        headChargeShotTimer_ = kHeadChargeShotInterval;
        return;
    }

    headChargeTimer_ += deltaTime;
    headChargeShotTimer_ += deltaTime;

    if (headChargeShotTimer_ < kHeadChargeShotInterval) {
        return;
    }

    headChargeShotTimer_ = 0.0f;
    FireChargedBullet();
    headChargeShotCount_ = headChargeShotCount_ + 1;

    if (headChargeShotCount_ >= kHeadChargeShotMax) {
        FinishHeadChargeAttack();
    }
}

void FearWormEnemy::StartHeadChargeAttack()
{
    isHeadChargeActive_ = true;
    isHeadChargeFiring_ = false;
    headChargeTimer_ = 0.0f;
    headChargeShotTimer_ = 0.0f;
    headChargeEffectTimer_ = 0.0f;
    headChargeShotCount_ = 0;
    fireTimer_ = 0;

    UpdateHeadAimDirection();

    EffectManager::GetInstance()->PlayEffect(
        "WormShotFlash",
        CalculateHeadMuzzlePosition());
}

void FearWormEnemy::FinishHeadChargeAttack()
{
    isHeadChargeActive_ = false;
    isHeadChargeFiring_ = false;
    headChargeTimer_ = 0.0f;
    headChargeShotTimer_ = 0.0f;
    headChargeEffectTimer_ = 0.0f;
    headChargeShotCount_ = 0;
    headChargeCooldownTimer_ = kHeadChargeCooldown;
}

void FearWormEnemy::UpdateHeadAimDirection()
{
    if (player_ == nullptr) {
        return;
    }

    if (segments_.empty()) {
        return;
    }

    Vector3 targetDirection =
        NormalizeSafe(player_->GetTranslate() - segments_[0].position);

    headAimDirection_ = NormalizeSafe(
        Lerp(
            headAimDirection_,
            targetDirection,
            0.18f));
}

void FearWormEnemy::FireBullet(const Vector3& position)
{
    std::unique_ptr<EnemyBullet> bullet = std::make_unique<NormalEnemyBullet>();
    bullet->Initialize(bulletModel_);

    Vector3 direction = NormalizeSafe(player_->GetTranslate() - position);
    Vector3 velocity {};
    velocity.x = direction.x * bulletSpeed_;
    velocity.y = direction.y * bulletSpeed_;
    velocity.z = direction.z * bulletSpeed_;

    bullet->SetTranslate(position);
    bullet->SetVelocity(velocity);

    enemyBullets_.push_back(std::move(bullet));

    EffectManager::GetInstance()->PlayEffect(
        "WormShotFlash",
        position);
}

void FearWormEnemy::FireChargedBullet()
{
    if (player_ == nullptr) {
        return;
    }

    if (bulletModel_ == nullptr) {
        return;
    }

    std::unique_ptr<EnemyBullet> bullet = std::make_unique<NormalEnemyBullet>();
    bullet->Initialize(bulletModel_);

    Vector3 position =
        CalculateHeadMuzzlePosition();

    Vector3 direction = NormalizeSafe(player_->GetTranslate() - position);

    Vector3 velocity {};
    velocity.x = direction.x * kChargedBulletSpeed;
    velocity.y = direction.y * kChargedBulletSpeed;
    velocity.z = direction.z * kChargedBulletSpeed;

    bullet->SetScale({
        kChargedBulletScale,
        kChargedBulletScale,
        kChargedBulletScale
    });
    bullet->SetColor({ 1.0f, 0.12f, 0.02f, 1.0f });
    bullet->SetEnableLighting(false);
    bullet->SetDamage(kChargedBulletDamage);
    bullet->SetCollisionRadius(kChargedBulletCollisionRadius);
    bullet->SetMaxLifeTime(5.8f);
    bullet->SetTranslate(position);
    bullet->SetVelocity(velocity);

    enemyBullets_.push_back(std::move(bullet));

    EffectManager::GetInstance()->PlayEffect(
        "WormShotFlash",
        position);
}

Vector3 FearWormEnemy::CalculateHeadMuzzlePosition() const
{
    Vector3 position =
        GetPosition();

    Vector3 direction =
        NormalizeSafe(headAimDirection_);

    position.x += direction.x * 4.2f;
    position.y += direction.y * 4.2f;
    position.z += direction.z * 4.2f;

    return position;
}

Vector3 FearWormEnemy::CalculateHeadLookRotation() const
{
    Vector3 direction =
        NormalizeSafe(headAimDirection_);

    Vector3 rotate {};
    rotate.x = -std::asin(direction.y);
    rotate.y = std::atan2(direction.x, direction.z);
    rotate.z = 0.0f;

    return rotate;
}

void FearWormEnemy::UpdateBullets()
{
    for (std::unique_ptr<EnemyBullet>& bullet : enemyBullets_) {
        bullet->Update();
    }
}

void FearWormEnemy::RemoveDeadBullets()
{
    for (size_t index = 0; index < enemyBullets_.size();) {
        if (!enemyBullets_[index]->IsAlive()) {
            enemyBullets_.erase(enemyBullets_.begin() + index);
        } else {
            index = index + 1;
        }
    }
}

void FearWormEnemy::PlayBodyBreakEffect(const Vector3& position)
{
    EffectManager::GetInstance()->PlayEffect(
        "WormBodyBreak",
        position);
}

void FearWormEnemy::PlayHeadGuardEffect(const Vector3& position)
{
    EffectManager::GetInstance()->PlayEffect(
        "WormHeadGuard",
        position);
}

void FearWormEnemy::PlayHeadVulnerableEffect(const Vector3& position)
{
    EffectManager::GetInstance()->PlayEffect(
        "WormHeadVulnerable",
        position);
}

void FearWormEnemy::UpdateDeathSequence()
{
    if (isDeathSequenceFinished_) {
        return;
    }

    deathSequenceTimer_ += kFrameTime;

    if (nextDeathSegmentIndex_ >= 0) {
        if (deathSequenceTimer_ < kDeathExplosionInterval) {
            return;
        }

        deathSequenceTimer_ = 0.0f;

        Segment& segment = segments_[nextDeathSegmentIndex_];
        if (segment.isAlive) {
            if (segment.isHead) {
                EffectManager::GetInstance()->PlayEffect(
                    "WormDeathExplosion",
                    segment.position);
            } else {
                PlayBodyBreakEffect(segment.position);
            }

            segment.isAlive = false;
        }

        nextDeathSegmentIndex_ = nextDeathSegmentIndex_ - 1;
        return;
    }

    if (deathSequenceTimer_ >= kDeathSequenceEndDelay) {
        isDeathSequenceFinished_ = true;
    }
}

float FearWormEnemy::CalculateHealthRate() const
{
    float currentHealth = 0.0f;
    float maxHealth = 0.0f;

    for (const Segment& segment : segments_) {
        if (segment.isHead) {
            maxHealth += kHeadHp;
        } else {
            maxHealth += kBodyHp;
        }

        if (!segment.isAlive) {
            continue;
        }

        if (segment.hp > 0.0f) {
            currentHealth += segment.hp;
        }
    }

    if (maxHealth <= 0.0001f) {
        return 0.0f;
    }

    float healthRate =
        currentHealth /
        maxHealth;

    if (healthRate < 0.0f) {
        healthRate = 0.0f;
    }

    if (healthRate > 1.0f) {
        healthRate = 1.0f;
    }

    return healthRate;
}

float FearWormEnemy::CalculateMovementSpeedRate() const
{
    float healthRate =
        CalculateHealthRate();

    float damageRate =
        1.0f -
        healthRate;

    return 1.0f + damageRate * 0.45f;
}

bool FearWormEnemy::HasAliveBodyParts() const
{
    for (size_t index = 1; index < segments_.size(); index = index + 1) {
        if (segments_[index].isAlive) {
            return true;
        }
    }

    return false;
}

bool FearWormEnemy::IsValidSegmentIndex(int32_t partIndex) const
{
    if (partIndex < 0) {
        return false;
    }

    if (partIndex >= static_cast<int32_t>(segments_.size())) {
        return false;
    }

    return true;
}

void FearWormEnemy::OnDeath()
{
    enemyBullets_.clear();
    isHeadChargeActive_ = false;
    isHeadChargeFiring_ = false;
    isDeathSequenceFinished_ = false;
    deathSequenceTimer_ = 0.0f;
    nextDeathSegmentIndex_ = static_cast<int32_t>(segments_.size()) - 1;
}

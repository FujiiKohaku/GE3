#include "App/Game/Boss/FearWormEnemy/FearWormEnemy.h"

#include "App/Game/Enemy/Bullet/NormalEnemyBullet.h"
#include "App/Game/Player/Player.h"
#include "Engine/3D/Object3d.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/Effect/EffectManager.h"
#include "Engine/math/MathStruct.h"

#include <cstddef>
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
constexpr float kBaseMoveSpeed = 0.10f;
constexpr float kBodyScale = 2.45f;
constexpr float kHeadScale = 3.45f;
constexpr float kTailScale = 2.15f;
constexpr float kCollisionRadiusScale = 0.90f;
constexpr float kInitialHeadChargeCooldown = 1.80f;
constexpr Vector3 kDefaultHeadAimDirection = { 0.0f, 0.0f, -1.0f };

constexpr float kOrbitAngularSpeed = 1.15f;
constexpr float kFallbackMoveXFrequency = 1.00f;
constexpr float kFallbackMoveXAmplitude = 6.0f;
constexpr float kFallbackMoveYFrequency = 1.30f;
constexpr float kFallbackMoveYAmplitude = 2.5f;
constexpr float kFallbackMoveZFrequency = 0.60f;
constexpr float kFallbackMoveZAmplitude = 3.0f;

constexpr float kEntryOffsetX = -34.0f;
constexpr float kEntryOffsetY = 18.0f;
constexpr float kEntryExtraForwardOffset = 22.0f;

constexpr float kOrbitRadiusX = 16.0f;
constexpr float kOrbitRadiusY = 7.0f;
constexpr float kOrbitHeight = 7.0f;
constexpr float kOrbitLateralSwayFrequency = 2.0f;
constexpr float kOrbitLateralSwayAmplitude = 2.0f;
constexpr float kOrbitDepthSwayFrequency = 0.80f;
constexpr float kOrbitDepthSwayAmplitude = 3.0f;

constexpr float kCoilAngleRate = 2.35f;
constexpr float kCoilBaseRadius = 8.0f;
constexpr float kCoilRadiusFrequency = 3.2f;
constexpr float kCoilRadiusAmplitude = 2.0f;
constexpr float kCoilHeight = 8.0f;
constexpr float kCoilVerticalScale = 0.75f;
constexpr float kCoilDepthFrequency = 0.50f;
constexpr float kCoilDepthAmplitude = 2.5f;

constexpr float kWeaveAngleRate = 1.35f;
constexpr float kWeaveWidth = 13.0f;
constexpr float kWeaveHeight = 7.0f;
constexpr float kWeaveVerticalFrequency = 2.0f;
constexpr float kWeaveVerticalAmplitude = 9.0f;
constexpr float kWeaveDepthAmplitude = 2.5f;

constexpr float kDriftHorizontalFrequency = 0.65f;
constexpr float kDriftHorizontalAmplitude = 8.0f;
constexpr float kDriftHeight = 8.0f;
constexpr float kDriftVerticalFrequency = 0.85f;
constexpr float kDriftVerticalAmplitude = 4.5f;
constexpr float kDriftDepthFrequency = 0.50f;
constexpr float kDriftDepthAmplitude = 2.0f;

constexpr float kInitialOrbitAngle = 2.35f;

constexpr Vector4 kHeadColor = { 0.30f, 0.88f, 1.0f, 1.0f };
constexpr Vector4 kVulnerableHeadColor = { 1.0f, 0.18f, 0.04f, 1.0f };
constexpr Vector4 kChargingHeadColor = { 1.0f, 0.48f, 0.08f, 1.0f };
constexpr Vector4 kFiringHeadColor = { 1.0f, 0.08f, 0.02f, 1.0f };
constexpr Vector4 kBodyColor = { 0.78f, 0.20f, 1.0f, 1.0f };
constexpr Vector4 kHitFlashColor = { 1.0f, 0.95f, 0.35f, 1.0f };
constexpr float kBodyPulseFrequency = 5.5f;
constexpr float kBodyPulseAmplitude = 0.08f;
constexpr float kChargePulseAdd = 0.12f;
constexpr float kChargePulseFrequency = 24.0f;
constexpr float kChargePulseAmplitude = 0.08f;
constexpr float kSegmentRotationOffset = 0.35f;

constexpr float kDamageHitFlashDuration = 0.10f;
constexpr float kGuardHitFlashDuration = 0.08f;
constexpr float kHeadAimFollowRate = 0.18f;
constexpr Vector4 kChargedBulletColor = { 1.0f, 0.12f, 0.02f, 1.0f };
constexpr float kChargedBulletLifeTime = 5.8f;
constexpr float kHeadMuzzleOffset = 4.2f;
constexpr float kMinimumHealth = 0.0001f;
constexpr float kMaximumMovementSpeedAdd = 0.45f;

}

void FearWormEnemy::Initialize(Model* model,Model* bulletModel,Player* player)
{
    bulletModel_ = bulletModel;
    player_ = player;
    hp_ = kHeadHp;
    moveSpeed_ = kBaseMoveSpeed;

    InitializeSegments(model); //
    SetPosition(transform_.translate);
}

void FearWormEnemy::InitializeSegments(Model* model)
{
	//体のセグメントを初期化
    segments_.clear();
    segments_.reserve(kWormSegmentCount);


	// 体作成
    for (int32_t index = 0; index < kWormSegmentCount; index = index + 1) {
        Segment segment {};
        segment.object = std::make_unique<Object3d>();
        segment.object->Initialize(Object3dManager::GetInstance());
        segment.object->SetModel(model);
        segment.object->SetEnableLighting(false);
        segment.isHead = false;
        segment.isAlive = true;
        segment.hp = kBodyHp;

        float scale = kBodyScale;
		if (index == 0) { // 頭のセグメント
            segment.isHead = true;
            segment.hp = kHeadHp;
            scale = kHeadScale;
        }

		if (index == kWormSegmentCount - 1) {// 尾のセグメント
            scale = kTailScale;
        }

        segment.scale = { scale, scale, scale };
        segment.radius = scale * kCollisionRadiusScale;

        segments_.push_back(std::move(segment));
    }
}

void FearWormEnemy::SetPosition(const Vector3& position)
{
    // 基準位置と行動状態を初期状態へ戻す。
    transform_.translate = position;
    startPosition_ = position;
    isParallelStarted_ = false;
    movementPattern_ = MovementPattern::Orbit;
    movementPatternTimer_ = 0.0f;
    isHeadChargeActive_ = false;
    isHeadChargeFiring_ = false;
    headChargeTimer_ = 0.0f;
    headChargeShotTimer_ = 0.0f;
    headChargeCooldownTimer_ = kInitialHeadChargeCooldown;
    headChargeEffectTimer_ = 0.0f;
    headChargeShotCount_ = 0;
    headAimDirection_ = kDefaultHeadAimDirection;
    // 全セグメントを基準位置から後方へ等間隔に並べる。
    for (size_t index = 0; index < segments_.size(); index = index + 1) {
        float offset = segmentSpacing_ * static_cast<float>(index);
        segments_[index].position = position;
        segments_[index].position.z -= offset;
    }
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
                kOrbitAngularSpeed *
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
        target.x += std::sin(moveTime_ * kFallbackMoveXFrequency) * kFallbackMoveXAmplitude;
        target.y += std::sin(moveTime_ * kFallbackMoveYFrequency) * kFallbackMoveYAmplitude;
        target.z += std::sin(moveTime_ * kFallbackMoveZFrequency) * kFallbackMoveZAmplitude;
    }

    segments_[0].position = Lerp(
        segments_[0].position,
        target,
        moveSpeed_ * movementSpeedRate);

    transform_.translate = segments_[0].position;

}

Vector3 FearWormEnemy::CalculateEntryStartPosition(const Vector3& playerPosition) const
{
    // プレイヤーの左上かつ前方を登場開始位置にする。
    Vector3 result = playerPosition;
    result.x += kEntryOffsetX;
    result.y += kEntryOffsetY;
    result.z += parallelForwardOffset_ + kEntryExtraForwardOffset;
    return result;
}

Vector3 FearWormEnemy::CalculateOrbitTargetPosition(const Vector3& playerPosition) const
{
    Vector3 result = playerPosition;

    // 基本の楕円軌道を計算する。
    float orbitX =
        std::cos(orbitAngle_) *
        kOrbitRadiusX;

    float orbitY =
        std::sin(orbitAngle_) *
        kOrbitRadiusY;

    // 単調な周回にならないよう、横揺れと奥行きの揺れを加える。
    float lateralSway =
        std::sin(orbitAngle_ * kOrbitLateralSwayFrequency) *
        kOrbitLateralSwayAmplitude;

    float depthSway =
        std::sin(orbitAngle_ * kOrbitDepthSwayFrequency) *
        kOrbitDepthSwayAmplitude;

    result.x += orbitX + lateralSway;
    result.y += kOrbitHeight + orbitY;
    result.z += parallelForwardOffset_ + depthSway;

    return result;
}

Vector3 FearWormEnemy::CalculateCoilTargetPosition(const Vector3& playerPosition) const
{
    Vector3 result = playerPosition;

    // 通常の軌道角を速めて、巻き付くような回転角を作る。
    float coilAngle =
        orbitAngle_ *
        kCoilAngleRate;

    // 時間経過で半径を伸縮させる。
    float radius =
        kCoilBaseRadius +
        std::sin(movementPatternTimer_ * kCoilRadiusFrequency) *
        kCoilRadiusAmplitude;

    result.x += std::cos(coilAngle) * radius;
    result.y += kCoilHeight + std::sin(coilAngle) * radius * kCoilVerticalScale;
    result.z += parallelForwardOffset_ + std::sin(coilAngle * kCoilDepthFrequency) * kCoilDepthAmplitude;

    return result;
}

Vector3 FearWormEnemy::CalculateWeaveTargetPosition(const Vector3& playerPosition) const
{
    Vector3 result = playerPosition;

    // 横方向と縦方向で異なる周期を使い、波打つ軌道を作る。
    float waveAngle =
        orbitAngle_ *
        kWeaveAngleRate;

    result.x += std::sin(waveAngle) * kWeaveWidth;
    result.y += kWeaveHeight + std::sin(waveAngle * kWeaveVerticalFrequency) * kWeaveVerticalAmplitude;
    result.z += parallelForwardOffset_ + std::cos(waveAngle) * kWeaveDepthAmplitude;

    return result;
}

Vector3 FearWormEnemy::CalculateDriftTargetPosition(const Vector3& playerPosition) const
{
    Vector3 result = playerPosition;

    // 各軸を異なる周期で揺らし、緩やかに漂う軌道を作る。
    result.x += std::sin(orbitAngle_ * kDriftHorizontalFrequency) * kDriftHorizontalAmplitude;
    result.y += kDriftHeight + std::sin(orbitAngle_ * kDriftVerticalFrequency) * kDriftVerticalAmplitude;
    result.z += parallelForwardOffset_ + std::cos(orbitAngle_ * kDriftDepthFrequency) * kDriftDepthAmplitude;

    return result;
}

Vector3 FearWormEnemy::CalculateMovementTargetPosition(const Vector3& playerPosition) const
{
    // 現在の移動状態に対応する計算へ振り分ける。
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
    // 登場用タイマーと移動パターンを初期状態へ戻す。
    isParallelStarted_ = true;
    enterTimer_ = 0.0f;
    orbitAngle_ = kInitialOrbitAngle;
    movementPattern_ = MovementPattern::Orbit;
    movementPatternTimer_ = 0.0f;

    // 頭部を登場開始位置へ移す。
    Vector3 entryPosition =
        CalculateEntryStartPosition(playerPosition);

    segments_[0].position = entryPosition;
    transform_.translate = entryPosition;
}

void FearWormEnemy::UpdateSegments()
{
    if (segments_.empty()) {
        return;
    }

    Segment* previousSegment = &segments_[0];

    for (size_t index = 1; index < segments_.size(); index = index + 1) {
        Segment& segment = segments_[index];

        if (!segment.isAlive) {
            continue;
        }

        Vector3 direction = Normalize(
            segment.position - previousSegment->position);

        segment.position =
            previousSegment->position +
            direction * segmentSpacing_;

        previousSegment = &segment;
    }
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
            color = kHeadColor;

            if (!HasAliveBodyParts()) {
                color = kVulnerableHeadColor;
            }

            if (isHeadChargeActive_) {
                color = kChargingHeadColor;
            }

            if (isHeadChargeFiring_) {
                color = kFiringHeadColor;
            }
        } else {
            color = kBodyColor;
        }

        if (segment.hitFlashTimer > 0.0f) {
            color = kHitFlashColor;
        }

        float pulse = 1.0f + std::sin(moveTime_ * kBodyPulseFrequency + static_cast<float>(index)) * kBodyPulseAmplitude;
        if (segment.isHead && isHeadChargeActive_) {
            pulse += kChargePulseAdd;
            pulse += std::sin(headChargeTimer_ * kChargePulseFrequency) * kChargePulseAmplitude;
        }

        Vector3 scale {};
        scale.x = segment.scale.x * pulse;
        scale.y = segment.scale.y * pulse;
        scale.z = segment.scale.z * pulse;

        Vector3 rotate {};
        rotate.y = moveTime_ + static_cast<float>(index) * kSegmentRotationOffset;
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

    // 生存中の部位だけを当たり判定として登録する。
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
    // 範囲外または破壊済みの部位にはダメージを通さない。
    if (!IsValidSegmentIndex(partIndex)) {
        return false;
    }

    const Segment& segment = segments_[partIndex];
    if (!segment.isAlive) {
        return false;
    }

    // 胴体が残っている間は頭部を無敵にする。
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

    // ダメージ無効の部位ならガード演出だけを出す。
    if (!IsCollisionPartDamageable(partIndex)) {
        OnCollisionPartGuarded(partIndex, segments_[partIndex].position);
        return;
    }

    // HPを減らし、まだ残っていれば被弾表示だけ更新する。
    Segment& segment = segments_[partIndex];
    segment.hp -= damage;
    segment.hitFlashTimer = kDamageHitFlashDuration;

    if (segment.hp > 0.0f) {
        return;
    }

    // 頭部のHPが尽きた場合はボス撃破へ移行する。
    if (segment.isHead) {
        SetDead(true);
        return;
    }

    // 胴体の場合は対象部位だけを破壊する。
    segment.isAlive = false;
    PlayBodyBreakEffect(segment.position);

    // 最後の胴体が壊れた瞬間に頭部の弱点化を通知する。
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

    // 対象部位を短時間点滅させ、命中位置にガード演出を出す。
    segments_[partIndex].hitFlashTimer = kGuardHitFlashDuration;
    PlayHeadGuardEffect(position);
}

void FearWormEnemy::Attack()
{
    // 登場前、攻撃対象なし、弾モデルなしの場合は攻撃しない。
    if (!isParallelStarted_) {
        return;
    }

    if (player_ == nullptr) {
        return;
    }

    if (bulletModel_ == nullptr) {
        return;
    }

    // 攻撃範囲外では進行中のチャージも解除する。
    Vector3 toPlayer = player_->GetTranslate() - GetPosition();
    float distance = Vector3Length(toPlayer);

    if (distance > kAttackRange) {
        if (isHeadChargeActive_) {
            FinishHeadChargeAttack();
        }
        return;
    }

    // 頭部の照準とチャージ攻撃を優先して更新する。
    UpdateHeadAimDirection();
    UpdateHeadChargeAttack(CalculateMovementSpeedRate());

    if (isHeadChargeActive_) {
        return;
    }

    // チャージ中でなければ通常弾の発射間隔を数える。
    fireTimer_ = fireTimer_ + 1;
    if (fireTimer_ < fireInterval_) {
        return;
    }

    fireTimer_ = 0;

    // 生存部位を順番に探し、見つかった最初の部位から発射する。
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
    // チャージ状態へ移行し、各タイマーと発射数を初期化する。
    isHeadChargeActive_ = true;
    isHeadChargeFiring_ = false;
    headChargeTimer_ = 0.0f;
    headChargeShotTimer_ = 0.0f;
    headChargeEffectTimer_ = 0.0f;
    headChargeShotCount_ = 0;
    fireTimer_ = 0;

    // 開始時点の照準を確定し、発射口にチャージ演出を出す。
    UpdateHeadAimDirection();

    EffectManager::GetInstance()->PlayEffect(
        "WormShotFlash",
        CalculateHeadMuzzlePosition());
}

void FearWormEnemy::FinishHeadChargeAttack()
{
    // チャージ状態を解除し、次回攻撃までの待機時間を設定する。
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
            kHeadAimFollowRate));
}

void FearWormEnemy::FireBullet(const Vector3& position)
{
    // 通常弾を生成し、発射位置からプレイヤーへの速度を設定する。
    std::unique_ptr<EnemyBullet> bullet = std::make_unique<NormalEnemyBullet>();
    bullet->Initialize(bulletModel_);

    Vector3 direction = NormalizeSafe(player_->GetTranslate() - position);
    Vector3 velocity {};
    velocity.x = direction.x * bulletSpeed_;
    velocity.y = direction.y * bulletSpeed_;
    velocity.z = direction.z * bulletSpeed_;

    bullet->SetTranslate(position);
    bullet->SetVelocity(velocity);

    // 管理リストへ追加して発射エフェクトを再生する。
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

    // 頭部の発射位置からプレイヤーへ向かう弾を生成する。
    std::unique_ptr<EnemyBullet> bullet = std::make_unique<NormalEnemyBullet>();
    bullet->Initialize(bulletModel_);

    Vector3 position =
        CalculateHeadMuzzlePosition();

    Vector3 direction = NormalizeSafe(player_->GetTranslate() - position);

    Vector3 velocity {};
    velocity.x = direction.x * kChargedBulletSpeed;
    velocity.y = direction.y * kChargedBulletSpeed;
    velocity.z = direction.z * kChargedBulletSpeed;

    // 通常弾より強い見た目、威力、当たり判定、寿命を設定する。
    bullet->SetScale({
        kChargedBulletScale,
        kChargedBulletScale,
        kChargedBulletScale
    });
    bullet->SetColor(kChargedBulletColor);
    bullet->SetEnableLighting(false);
    bullet->SetDamage(kChargedBulletDamage);
    bullet->SetCollisionRadius(kChargedBulletCollisionRadius);
    bullet->SetMaxLifeTime(kChargedBulletLifeTime);
    bullet->SetTranslate(position);
    bullet->SetVelocity(velocity);

    // 管理リストへ追加して発射エフェクトを再生する。
    enemyBullets_.push_back(std::move(bullet));

    EffectManager::GetInstance()->PlayEffect(
        "WormShotFlash",
        position);
}

Vector3 FearWormEnemy::CalculateHeadMuzzlePosition() const
{
    // 頭部中心から照準方向へ発射口分だけ前進させる。
    Vector3 position =
        GetPosition();

    Vector3 direction =
        NormalizeSafe(headAimDirection_);

    position.x += direction.x * kHeadMuzzleOffset;
    position.y += direction.y * kHeadMuzzleOffset;
    position.z += direction.z * kHeadMuzzleOffset;

    return position;
}

Vector3 FearWormEnemy::CalculateHeadLookRotation() const
{
    // 正規化した照準方向をピッチ角とヨー角へ変換する。
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

    // 全部位の最大HPと、生存部位の残りHPを合計する。
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

    if (maxHealth <= kMinimumHealth) {
        return 0.0f;
    }

    // 最大HPに対する割合を求め、0～1の範囲に収める。
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
    // HPが減るほど移動と攻撃の速度倍率を上げる。
    float healthRate =
        CalculateHealthRate();

    float damageRate =
        1.0f -
        healthRate;

    return 1.0f + damageRate * kMaximumMovementSpeedAdd;
}

bool FearWormEnemy::HasAliveBodyParts() const
{
    // 先頭の頭部を除き、生存している胴体を探す。
    for (size_t index = 1; index < segments_.size(); index = index + 1) {
        if (segments_[index].isAlive) {
            return true;
        }
    }

    return false;
}

bool FearWormEnemy::IsValidSegmentIndex(int32_t partIndex) const
{
    // 負数と配列末尾より後ろの番号を無効とする。
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
    // 残っている弾と攻撃状態を停止する。
    enemyBullets_.clear();
    isHeadChargeActive_ = false;
    isHeadChargeFiring_ = false;
    // 尻尾側の部位から順番に処理する撃破シーケンスを開始する。
    isDeathSequenceFinished_ = false;
    deathSequenceTimer_ = 0.0f;
    nextDeathSegmentIndex_ = static_cast<int32_t>(segments_.size()) - 1;
}

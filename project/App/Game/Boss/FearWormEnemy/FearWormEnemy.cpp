#include "App/Game/Boss/FearWormEnemy/FearWormEnemy.h"
#include "FearWormState.h"

#include "App/Game/Enemy/Bullet/NormalEnemyBullet.h"
#include "App/Game/Enemy/Bullet/TrackingEnemyBullet.h"
#include "App/Game/Player/Player.h"
#include "Engine/Camera/Camera.h"
#include "Engine/3D/Object3d.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/Effect/EffectManager.h"
#include "Engine/Time/TimeManager.h"
#include "Engine/math/MathStruct.h"
#include "Engine/3D/ModelManager.h"

#include <cmath>
#include <cstddef>
#include <utility>

namespace {
constexpr int32_t kWormSegmentCount = 10;
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

// 追尾ミサイル攻撃用の定数

constexpr float kFallbackMoveXFrequency = 1.00f;
constexpr float kFallbackMoveXAmplitude = 6.0f;
constexpr float kFallbackMoveYFrequency = 1.30f;
constexpr float kFallbackMoveYAmplitude = 2.5f;
constexpr float kFallbackMoveZFrequency = 0.60f;
constexpr float kFallbackMoveZAmplitude = 3.0f;

constexpr float kEntryOffsetX = -34.0f;
constexpr float kEntryOffsetY = 18.0f;
constexpr float kEntryExtraForwardOffset = 22.0f;

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
constexpr Vector4 kChargedBulletColor = { 1.0f, 0.353f, 0.239f, 1.0f };
constexpr float kChargedBulletLifeTime = 20.0f;
constexpr float kHeadMuzzleOffset = 4.2f;
constexpr float kMinimumHealth = 0.0001f;
constexpr float kMaximumMovementSpeedAdd = 0.45f;

}

void FearWormEnemy::Initialize(Model* model, Model* bulletModel, Player* player)
{
    bulletModel_ = bulletModel;
    player_ = player;
    SetHealth(kHeadHp);
    moveSpeed_ = kBaseMoveSpeed;

    isMadModeActive_ = false;
    isMadModeFinished_ = false;
    madModeTimer_ = 0.0f;

    InitializeSegments(model);
    InitializeBeam();
    SetPosition(transform_.translate);
}

void FearWormEnemy::InitializeSegments(Model* model)
{
    // 体のセグメントを初期化
    segments_.clear();
    segments_.reserve(kWormSegmentCount);

    // 体作成
    for (int32_t index = 0; index < kWormSegmentCount; index = index + 1) {
        Segment segment { };
        segment.object = std::make_unique<Object3d>();
        segment.object->Initialize(Object3dManager::GetInstance());
        segment.object->SetMaterial("resources/Shaders/Object3D/ShadowStandard");
        segment.object->SetCastShadow(true);
        segment.object->SetReceiveShadow(true);
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

        if (index == kWormSegmentCount - 1) { // 尾のセグメント
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
    ChangeState(BossState::Wait);
    ChangeMovementPattern(MovementPattern::Orbit);
    movementPatternTimer_ = 0.0f;
    isHeadChargeActive_ = false;
    isHeadChargeFiring_ = false;
    headChargeTimer_ = 0.0f;
    headChargeShotTimer_ = 0.0f;
    headChargeCooldownTimer_ = kInitialHeadChargeCooldown;
    headChargeEffectTimer_ = 0.0f;
    headChargeShotCount_ = 0;
    headAimDirection_ = kDefaultHeadAimDirection;

    // 追尾ミサイル関連の初期化
    missileAttackTimer_ = 0.0f;
    isMissileAttackActive_ = false;
    missilePhaseTimer_ = 0.0f;
    hasFiredMissiles_ = false;
    isFiringMissilesSequence_ = false;
    missileFireTimer_ = 0.0f;
    missileShotCount_ = 0;
    missileTargetIndices_.clear();
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
    // 外部から撃破された場合も、死亡状態への移行は一度だけ行う。
    if (IsDead()) {
        ChangeState(BossState::Dead);
    }

    moveTime_ += TimeManager::GetInstance()->GetDeltaTime();
    currentState_->Update(*this);

    // 各セグメントの移動・回転・拡縮・カラー等の更新
    // ※ Wait中も画面外で追従描画するために常に実行
    UpdateSegments();
    UpdateSegmentObjects();

    // 全胴体破壊時の頭部弱点化チェック
    if (GetBossState() == BossState::Battle && !vulnerableEffectPlayed_ && !HasAliveBodyParts()) {
        vulnerableEffectPlayed_ = true;
        PlayHeadVulnerableEffect(GetPosition());
    }
}

Vector3 FearWormEnemy::FallbackTarget()
{
    Vector3 target = startPosition_;
    target.x += std::sin(moveTime_ * kFallbackMoveXFrequency) * kFallbackMoveXAmplitude;
    target.y += std::sin(moveTime_ * kFallbackMoveYFrequency) * kFallbackMoveYAmplitude;
    target.z += std::sin(moveTime_ * kFallbackMoveZFrequency) * kFallbackMoveZAmplitude;
    return target;
}

Vector3 FearWormEnemy::EntryTarget(const Vector3& playerPosition)
{
    enterTimer_ += TimeManager::GetInstance()->GetDeltaTime();
    float enterRate = enterTimer_ / enterDuration_;
    if (enterRate > 1.0f) {
        enterRate = 1.0f;
    }
    // 登場演出中は、登場開始位置から目標位置まで線形補間する
    return Lerp(EntryStartPosition(playerPosition), MovementTargetPosition(playerPosition), enterRate);
}

Vector3 FearWormEnemy::BattleTarget(const Vector3& playerPosition, float rate)
{
    UpdateMovementPattern(rate);

    Vector3 adjustedPlayerPosition = playerPosition;

    // プレイヤーのカメラのピッチ角（上下の角度）を取得し、基準とするY座標を補正する
    if (player_ != nullptr && player_->GetCamera() != nullptr) {
        Camera* camera = player_->GetCamera();
        
        // カメラのピッチ角を取得（ラジアン）
        // 上を向くと pitch < 0 になるため、符号を反転します
        float pitch = -camera->GetRotate().x;

        // プレイヤーとボスの奥行き（Z方向）の距離
        float distanceZ = std::abs(playerPosition.z - GetPosition().z);

        // プレイヤーから見たカメラの視線の高低差（Yオフセット）を計算
        float lookOffsetY = distanceZ * std::sin(pitch);

        // ボスの基準高度を補正
        adjustedPlayerPosition.y += lookOffsetY;
    }

    return MovementTargetPosition(adjustedPlayerPosition);
}

Vector3 FearWormEnemy::EntryStartPosition(const Vector3& playerPosition) const
{
    // プレイヤーの左上かつ前方を登場開始位置にする。
    Vector3 result = playerPosition;
    result.x += kEntryOffsetX;
    result.y += kEntryOffsetY;
    result.z += parallelForwardOffset_ + kEntryExtraForwardOffset;
    return result;
}

Vector3 FearWormEnemy::MovementTargetPosition(const Vector3& playerPosition) const
{
    const FearWormMovementContext context { playerPosition, orbitAngle_, movementPatternTimer_,
        parallelForwardOffset_, madModeTimer_, spiralAngle_ };
    return movement_->CalculateTarget(context);
}

void FearWormEnemy::UpdateMovementPattern(float movementSpeedRate)
{
    if (!movement_->AdvancesAutomatically()) return;
    movementPatternTimer_ += TimeManager::GetInstance()->GetDeltaTime() * movementSpeedRate;
    if (movementPatternTimer_ >= movementPatternDuration_) {
        ChangeMovementPattern(movement_->GetNext());
    }
}

void FearWormEnemy::ChangeMovementPattern(MovementPattern pattern)
{
    FearWormMovementState state { movementPatternTimer_, lineTransitionTimer_ };
    movement_->Exit(state);
    movement_ = &GetFearWormMovement(pattern);
    movement_->Enter(state);
}

void FearWormEnemy::UpdateSegments()
{
    if (segments_.empty()) {
        return;
    }

    const float blendT = movement_->UpdateLineBlend(
        { movementPatternTimer_, lineTransitionTimer_ }, TimeManager::GetInstance()->GetDeltaTime());
    Segment* previousSegment = &segments_[0];
    int32_t aliveOrder = 0;

    for (size_t index = 1; index < segments_.size(); index = index + 1) {
        Segment& segment = segments_[index];

        if (!segment.isAlive) {
            continue;
        }
        aliveOrder++;

        // 1. 通常の追従目標位置 (縦並び - 元の引き戻し拘束)
        Vector3 diff = segment.position - previousSegment->position;
        Vector3 direction = Normalize(diff);
        Vector3 posNormal = previousSegment->position + direction * segmentSpacing_;

        // 2. Line状態の目標位置 (横並び＋うねうね)
        float waveScaleY = 3.2f;
        float waveScaleZ = 2.0f;
        float waveFrequency = 4.2f;

        Vector3 posLine = {
            segments_[0].position.x - (segmentSpacing_ * static_cast<float>(aliveOrder)),
            segments_[0].position.y + std::sin(moveTime_ * waveFrequency + static_cast<float>(aliveOrder) * 0.8f) * waveScaleY,
            segments_[0].position.z + std::cos(moveTime_ * (waveFrequency * 0.8f) + static_cast<float>(aliveOrder) * 0.6f) * waveScaleZ
        };

        // 3. 通常位置とLine位置をブレンドして直接適用する
        // blendT = 0.0f のときは posNormal (従来と100%同一の挙動) になります
        segment.position = Lerp(posNormal, posLine, blendT);

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
            segment.hitFlashTimer -= TimeManager::GetInstance()->GetDeltaTime();
            if (segment.hitFlashTimer < 0.0f) {
                segment.hitFlashTimer = 0.0f;
            }
        }

        Vector4 color { };
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

        Vector3 scale { };
        scale.x = segment.scale.x * pulse;
        scale.y = segment.scale.y * pulse;
        scale.z = segment.scale.z * pulse;

        Vector3 rotate { };
        if (segment.isHead && GetBossState() == BossState::Dead) {
            // 死亡中は死亡状態の更新で回転と座標を設定しUpdateも呼ぶため、共通の上書き処理をバイパスする
            segment.object->SetScale(scale);
            segment.object->SetColor(color);
            continue;
        }

        rotate.y = moveTime_ + static_cast<float>(index) * kSegmentRotationOffset;
        bool isBeamAiming = !HasAliveBodyParts() && (beamState_ == BeamState::Charge || beamState_ == BeamState::Fire || beamState_ == BeamState::FadeOut);
        if (segment.isHead && (isHeadChargeActive_ || isBeamAiming)) {
            rotate = HeadLookRotation();
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
    if (GetBossState() != BossState::Wait) {
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

    DrawBeam();
}

void FearWormEnemy::DrawShadow(ShadowMapRenderer& renderer)
{
    if (GetBossState() == BossState::Wait) { return; }
    for (Segment& segment : segments_) {
        if (segment.isAlive && segment.object != nullptr) { segment.object->DrawShadow(renderer); }
    }
}

void FearWormEnemy::SubmitRaytracing(DxrRenderer& renderer) {
    if (GetBossState() == BossState::Wait) { return; }
    for (const Segment& segment : segments_) {
        if (segment.isAlive && segment.object) { segment.object->SubmitRaytracing(renderer); }
    }
    if (!segments_.empty() && segments_[0].isAlive && beamPlane_
        && (beamState_ == BeamState::Charge || beamState_ == BeamState::Fire || beamState_ == BeamState::FadeOut)) {
        beamPlane_->SubmitRaytracing(renderer);
    }
}

bool FearWormEnemy::IsDeathSequenceFinished() const
{
    return isDeathSequenceFinished_;
}

void FearWormEnemy::GetCollisionParts(std::vector<EnemyCollisionPart>& parts) const
{
    if (GetBossState() == BossState::Wait) {
        return;
    }

    // 生存中の部位だけを当たり判定として登録する。
    for (size_t index = 0; index < segments_.size(); index = index + 1) {
        const Segment& segment = segments_[index];

        if (!segment.isAlive) {
            continue;
        }

        EnemyCollisionPart part { };
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
    if (IsDead() || !std::isfinite(damage) || damage <= 0.0f) { return; }
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
    if (GetBossState() == BossState::Wait) {
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
    UpdateHeadChargeAttack(MovementSpeedRate());

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
    float deltaTime = TimeManager::GetInstance()->GetDeltaTime() * attackSpeedRate;

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
                HeadMuzzlePosition());
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
        HeadMuzzlePosition());
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

    Vector3 targetDirection = NormalizeSafe(player_->GetTranslate() - segments_[0].position);

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
    bullet->SetFogEnabled(false);
    bullet->Initialize(bulletModel_);

    Vector3 direction = NormalizeSafe(player_->GetTranslate() - position);
    Vector3 velocity { };
    velocity.x = direction.x * bulletSpeed_;
    velocity.y = direction.y * bulletSpeed_;
    velocity.z = direction.z * bulletSpeed_;

    bullet->SetTranslate(position);
    bullet->SetVelocity(velocity);

    // 管理リストへ追加して発射エフェクトを再生する。
    AddEnemyBullet(std::move(bullet));

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
    bullet->SetFogEnabled(false);
    bullet->Initialize(bulletModel_);

    Vector3 position = HeadMuzzlePosition();

    Vector3 direction = NormalizeSafe(player_->GetTranslate() - position);

    Vector3 velocity { };
    velocity.x = direction.x * kChargedBulletSpeed;
    velocity.y = direction.y * kChargedBulletSpeed;
    velocity.z = direction.z * kChargedBulletSpeed;

    // 通常弾より強い見た目、威力、当たり判定、寿命を設定する。
    bullet->SetScale({ kChargedBulletScale,
        kChargedBulletScale,
        kChargedBulletScale });
    bullet->SetColor(kChargedBulletColor);
    bullet->SetDamage(kChargedBulletDamage);
    bullet->SetCollisionRadius(kChargedBulletCollisionRadius);
    bullet->SetMaxLifeTime(kChargedBulletLifeTime);
    bullet->SetTranslate(position);
    bullet->SetVelocity(velocity);

    // 管理リストへ追加して発射エフェクトを再生する。
    AddEnemyBullet(std::move(bullet));

    EffectManager::GetInstance()->PlayEffect(
        "WormShotFlash",
        position);
}

Vector3 FearWormEnemy::HeadMuzzlePosition() const
{
    // 頭部中心から照準方向へ発射口分だけ前進させる。
    Vector3 position = GetPosition();

    Vector3 direction = NormalizeSafe(headAimDirection_);

    position.x += direction.x * kHeadMuzzleOffset;
    position.y += direction.y * kHeadMuzzleOffset;
    position.z += direction.z * kHeadMuzzleOffset;

    return position;
}

Vector3 FearWormEnemy::HeadLookRotation() const
{
    // 正規化した照準方向をピッチ角とヨー角へ変換する。
    Vector3 direction = NormalizeSafe(headAimDirection_);

    Vector3 rotate { };
    rotate.x = -std::asin(direction.y);
    rotate.y = std::atan2(direction.x, direction.z);
    rotate.z = 0.0f;

    return rotate;
}

void FearWormEnemy::PlayBodyBreakEffect(const Vector3& position)
{
    EffectManager::GetInstance()->PlayEffect("WormBodyBreak", position);
}

void FearWormEnemy::PlayHeadGuardEffect(const Vector3& position)
{
    EffectManager::GetInstance()->PlayEffect("WormHeadGuard", position);
}

void FearWormEnemy::PlayHeadVulnerableEffect(const Vector3& position)
{
    EffectManager::GetInstance()->PlayEffect("WormHeadVulnerable", position);
}

float FearWormEnemy::HealthRate() const
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
    float healthRate = currentHealth / maxHealth;

    if (healthRate < 0.0f) {
        healthRate = 0.0f;
    }

    if (healthRate > 1.0f) {
        healthRate = 1.0f;
    }

    return healthRate;
}

float FearWormEnemy::MovementSpeedRate() const
{
    // HPが減るほど移動と攻撃の速度倍率を上げる。
    float healthRate = HealthRate();

    float damageRate = 1.0f - healthRate;

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
    ChangeState(BossState::Dead);
}

void FearWormEnemy::FireSingleMissile(size_t segmentIndex)
{
    if (player_ == nullptr || bulletModel_ == nullptr) {
        return;
    }

    std::unique_ptr<TrackingEnemyBullet> bullet = std::make_unique<TrackingEnemyBullet>();
    bullet->Initialize(bulletModel_);
    bullet->SetPlayer(player_);

    // 発射座標を設定
    bullet->SetTranslate(segments_[segmentIndex].position);

    // 発射エフェクトの再生
    EffectManager::GetInstance()->PlayEffect("WormShotFlash", segments_[segmentIndex].position);

    // ボスの弾リストに追加
    AddEnemyBullet(std::move(bullet));
}

namespace {
// 任意軸まわりの回転行列を生成する関数 (ロドリゲスの回転公式)
Matrix4x4 MakeRotateAxisAngleMatrix(const Vector3& axis, float angle)
{
    Vector3 u = NormalizeSafe(axis);
    float c = std::cos(angle);
    float s = std::sin(angle);
    float t = 1.0f - c;

    Matrix4x4 mat {};
    mat.m[0][0] = t * u.x * u.x + c;
    mat.m[0][1] = t * u.x * u.y + s * u.z;
    mat.m[0][2] = t * u.x * u.z - s * u.y;
    mat.m[0][3] = 0.0f;

    mat.m[1][0] = t * u.x * u.y - s * u.z;
    mat.m[1][1] = t * u.y * u.y + c;
    mat.m[1][2] = t * u.y * u.z + s * u.x;
    mat.m[1][3] = 0.0f;

    mat.m[2][0] = t * u.x * u.z + s * u.y;
    mat.m[2][1] = t * u.y * u.z - s * u.x;
    mat.m[2][2] = t * u.z * u.z + c;
    mat.m[2][3] = 0.0f;

    mat.m[3][0] = 0.0f;
    mat.m[3][1] = 0.0f;
    mat.m[3][2] = 0.0f;
    mat.m[3][3] = 1.0f;

    return mat;
}
}

void FearWormEnemy::UpdateSpiralBarrage()
{
    // 最初の2秒間（威嚇中）は弾を撃たずにプレイヤーに猶予を与える
    if (madModeTimer_ < 2.0f) {
        return;
    }

    barrageFireTimer_++;
    if (barrageFireTimer_ < 3) {
        return;
    }
    barrageFireTimer_ = 0;

    Vector3 headPos = GetPosition();
    Vector3 playerPos = player_->GetTranslate();
    
    // ボスからプレイヤーへの方向ベクトルを基準にする（プレイヤー狙い補正）
    Vector3 toPlayer = NormalizeSafe(playerPos - headPos);

    // toPlayer に直交する直交座標系 (right, up) を構築
    Vector3 tempUp = { 0.0f, 1.0f, 0.0f };
    if (std::abs(toPlayer.y) > 0.99f) {
        tempUp = { 1.0f, 0.0f, 0.0f };
    }
    Vector3 right = NormalizeSafe(Cross(tempUp, toPlayer));
    Vector3 up = NormalizeSafe(Cross(toPlayer, right));

    float spread = (std::sin(barrageAngle_ * 2.5f) * 0.22f) + 0.22f; // 0.0(正確に狙う)〜0.44(周囲に拡散)を変動させる
    constexpr float kPi = 3.1415926535f;

    // プレイヤー方向を軸とした直交平面上で対角線上に広がる2つの螺旋オフセットベクトル
    Vector3 offset1 = right * (std::cos(barrageAngle_) * spread) + up * (std::sin(barrageAngle_) * spread);
    Vector3 offset2 = right * (std::cos(barrageAngle_ + kPi) * spread) + up * (std::sin(barrageAngle_ + kPi) * spread);

    // プレイヤー方向ベクトルと螺旋オフセットを合成して最終的な発射方向を決定
    Vector3 dir1 = NormalizeSafe(toPlayer + offset1);
    Vector3 dir2 = NormalizeSafe(toPlayer + offset2);

    FireDirectionalBullet(headPos, dir1);
    FireDirectionalBullet(headPos, dir2);
}

void FearWormEnemy::FireDirectionalBullet(const Vector3& position, const Vector3& direction)
{
    std::unique_ptr<EnemyBullet> bullet = std::make_unique<NormalEnemyBullet>();
    bullet->SetFogEnabled(false);
    bullet->Initialize(bulletModel_);

    Vector3 velocity { };
    velocity.x = direction.x * bulletSpeed_;
    velocity.y = direction.y * bulletSpeed_;
    velocity.z = direction.z * bulletSpeed_;

    bullet->SetTranslate(position);
    bullet->SetVelocity(velocity);

    AddEnemyBullet(std::move(bullet));

    EffectManager::GetInstance()->PlayEffect("WormShotFlash", position);
}

void FearWormEnemy::InitializeBeam()
{
    // ビーム用クロスPlaneモデルのロード
    Model* beamModel = ModelManager::GetInstance()->CreateBeamCross("resources/Textures/beam.jpg");

    // ビームオブジェクトの初期化
    beamPlane_ = std::make_unique<Object3d>();
    beamPlane_->Initialize(Object3dManager::GetInstance());
    beamPlane_->SetModel(beamModel);
    beamPlane_->SetEnableLighting(false);

    // 初期状態の設定
    currentBeamState_ = nullptr;
    ChangeBeamState(BeamState::Wait);
    beamTimer_ = 0.0f;
    beamUVScrollOffset_ = 0.0f;
    beamCurrentLength_ = 0.0f;
    beamCurrentWidth_ = 0.0f;
    beamRotateTheta_ = 0.0f;
}

void FearWormEnemy::UpdateBeamAttack()
{
    if (player_ == nullptr) {
        return;
    }

    float deltaTime = TimeManager::GetInstance()->GetDeltaTime();
    beamTimer_ += deltaTime;

    // UVスクロールオフセットを更新
    constexpr float kUVScrollSpeed = -6.5f; // より高速に流す
    beamUVScrollOffset_ += kUVScrollSpeed * deltaTime;
    if (beamUVScrollOffset_ < -1.0f) {
        beamUVScrollOffset_ += 1.0f;
    }

    // UVスクロール行列を作成してマテリアルにセット
    Vector3 uvTranslation = { 0.0f, beamUVScrollOffset_, 0.0f }; // V方向にスクロール
    Matrix4x4 uvMat = MatrixMath::MakeTranslateMatrix(uvTranslation);
    if (beamPlane_->GetMaterial()) {
        beamPlane_->SetUvTransform(uvMat);
    }

    // ビームの自転角度を更新
    constexpr float kRotateSpeed = 45.0f; // 高速自転（うねりの表現を強化）
    beamRotateTheta_ += kRotateSpeed * deltaTime;
    constexpr float kPi = 3.1415926535f;
    if (beamRotateTheta_ > kPi * 2.0f) {
        beamRotateTheta_ -= kPi * 2.0f;
    }

    // プレイヤーの座標
    Vector3 headPos = HeadMuzzlePosition();

    // 照準を更新する前のマズル位置を、予兆・発射エフェクトで共有する。
    beamMuzzlePosition_ = headPos;
    if (!currentBeamState_) ChangeBeamState(beamState_);
    currentBeamState_->Update(*this, deltaTime);

    // 1. 射向ベクトル (Lerpでゆっくり追従しているheadAimDirection_をそのままビームの進行方向軸とする)
    Vector3 zAxis = headAimDirection_;
    float length = beamCurrentLength_;
    if (length < 0.001f) length = 0.001f;

    // 2. 正規直交基底の作成 (仮の上方向と射向ベクトルの外積から右方向を算出)
    Vector3 tempUp = { 0.0f, 1.0f, 0.0f };
    if (std::abs(zAxis.y) > 0.999f) {
        tempUp = { 0.0f, 0.0f, 1.0f }; // 真上・真下を向いている場合のフォールバック
    }
    
    Vector3 xAxis = NormalizeSafe(Cross(tempUp, zAxis));
    Vector3 yAxis = Cross(zAxis, xAxis); // すでに直交かつ正規化されている

    // 3. 進行方向(zAxis)まわりの自転適用 (xAxis と yAxis を theta で回転)
    float cosT = std::cos(beamRotateTheta_);
    float sinT = std::sin(beamRotateTheta_);

    Vector3 xAxisRot = {
        xAxis.x * cosT + yAxis.x * sinT,
        xAxis.y * cosT + yAxis.y * sinT,
        xAxis.z * cosT + yAxis.z * sinT
    };

    Vector3 yAxisRot = {
        -xAxis.x * sinT + yAxis.x * cosT,
        -xAxis.y * sinT + yAxis.y * cosT,
        -xAxis.z * sinT + yAxis.z * cosT
    };

    // 4. 回転行列の直接構築 (基底ベクトルを直接代入)
    Matrix4x4 matRot = MatrixMath::MakeIdentity4x4();
    matRot.m[0][0] = xAxisRot.x;  matRot.m[0][1] = xAxisRot.y;  matRot.m[0][2] = xAxisRot.z;
    matRot.m[1][0] = yAxisRot.x;  matRot.m[1][1] = yAxisRot.y;  matRot.m[1][2] = yAxisRot.z;
    matRot.m[2][0] = zAxis.x;     matRot.m[2][1] = zAxis.y;     matRot.m[2][2] = zAxis.z;

    // 5. スケールと平行移動の合成 (Scale -> Rotate -> Translate)
    Matrix4x4 matScale = MatrixMath::Matrix4x4MakeScaleMatrix({ beamCurrentWidth_, beamCurrentWidth_, length });
    Matrix4x4 matTrans = MatrixMath::MakeTranslateMatrix(headPos);

    Matrix4x4 worldMat = MatrixMath::Multiply(MatrixMath::Multiply(matScale, matRot), matTrans);

    // 6. ワールド行列を直接設定
    beamPlane_->SetCustomWorldMatrix(worldMat);
    beamPlane_->Update();
}

void FearWormEnemy::DrawBeam()
{
    if (GetBossState() == BossState::Wait || segments_.empty()) {
        return;
    }

    if (!segments_[0].isAlive) {
        return;
    }

    if (beamState_ == BeamState::Charge || beamState_ == BeamState::Fire || beamState_ == BeamState::FadeOut) {
        if (beamPlane_) {
            // 加算ブレンドを適用して、黒背景を透過させ発光感を出す
            Object3dManager::GetInstance()->SetBlendMode(kBlendModeAdd);
            beamPlane_->Draw();
            // 通常ブレンドに戻す
            Object3dManager::GetInstance()->SetBlendMode(kBlendModeNormal);
        }
    }
}

void FearWormEnemy::CheckBeamCollision()
{
    isBeamHittingPlayer_ = false; // 毎フレーム初期化

    if (player_ == nullptr || beamCurrentLength_ <= 0.0f || beamCurrentWidth_ <= 0.0f) {
        return;
    }

    // ビームの始点と終点
    Vector3 beamStart = HeadMuzzlePosition();
    Vector3 beamEnd = beamStart + headAimDirection_ * beamCurrentLength_;

    // プレイヤーの座標と半径
    Vector3 playerPos = player_->GetTranslate();
    float playerRadius = 1.0f;
    float beamRadius = beamCurrentWidth_ * 0.5f;

    // 線分 AB (ビーム) と点 C (プレイヤー) の最短距離を求める
    Vector3 AB = beamEnd - beamStart;
    Vector3 AC = playerPos - beamStart;

    float abLenSq = Vector3LengthSquared(AB);
    float t = 0.0f;
    if (abLenSq > 0.0f) {
        t = Dot(AC, AB) / abLenSq;
        t = std::clamp(t, 0.0f, 1.0f);
    }

    // 最短点 P
    Vector3 P = beamStart + AB * t;

    // 最短距離の2乗
    Vector3 diff = playerPos - P;
    float distanceSq = Vector3LengthSquared(diff);

    // 衝突半径の合計の2乗
    float hitRadius = beamRadius + playerRadius;
    float hitRadiusSq = hitRadius * hitRadius;

    if (distanceSq <= hitRadiusSq) {
        isBeamHittingPlayer_ = true; // プレイヤーへの接触状態をON

        constexpr int kBeamDamage = 1;
        player_->ApplyDamage(kBeamDamage); // HP減少は無敵時間が適用される

        // 無敵時間に関わらず、接触している間は被弾火花エフェクトを3フレームに1回に間引いて発生させる (FPS低下防止)
        beamHitEffectFrameCount_++;
        if (beamHitEffectFrameCount_ >= 3) {
            beamHitEffectFrameCount_ = 0;
            EffectManager::GetInstance()->PlayEffect("DamageHit", playerPos);
        }
    } else {
        beamHitEffectFrameCount_ = 0;
    }
}

float FearWormEnemy::GetHeadHpFraction() const
{
    if (segments_.empty()) {
        return 0.0f;
    }
    return (std::max)(0.0f, segments_[0].hp) / kHeadHp;
}

float FearWormEnemy::GetBodyHpFraction() const
{
    if (segments_.size() <= 1) {
        return 0.0f;
    }
    
    float totalMaxHp = (segments_.size() - 1) * kBodyHp;
    float currentTotalHp = 0.0f;
    
    for (size_t index = 1; index < segments_.size(); ++index) {
        if (segments_[index].isAlive) {
            currentTotalHp += (std::max)(0.0f, segments_[index].hp);
        }
    }
    
    return currentTotalHp / totalMaxHp;
}

void FearWormEnemy::ChangeBeamState(BeamState state)
{
    ChangeBeamState(GetBeamState(state));
}

void FearWormEnemy::ChangeBeamState(const BaseBeamState& state)
{
    if (currentBeamState_ == &state) return;
    if (currentBeamState_) currentBeamState_->Exit(*this);
    currentBeamState_ = &state;
    beamState_ = state.GetType();
    currentBeamState_->Enter(*this);
}

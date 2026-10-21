// 戦闘行動の切り替えと、共通の座標・弾生成・描画処理。
#include "FearWormBattleState.h"
#include "../FearWormEnemy.h"
#include "Engine/3D/Object3d.h"
#include "Engine/Time/TimeManager.h"
#include "App/Game/Player/Player.h"
#include "Engine/Effect/EffectManager.h"
#include "Engine/Math/MathStruct.h"
#include <algorithm>
#include <cmath>
#include "Engine/Camera/Camera.h"
#include "App/Game/Enemy/Bullet/Types/NormalEnemyBullet.h"
#include <utility>

void FearWormBattleState::Update(FearWormEnemy& boss)
{
    if (boss.player_ == nullptr || boss.segments_.empty()) {
        return;
    }
    if (currentAction_ == nullptr) {
        ChangeAction(ActionType::OrbitShot, boss);
    }
    if (!boss.HasAliveBodyParts()) {
        ChangeAction(ActionType::Beam, boss);
    } else if (boss.HealthRate() <= 0.30f && !hasTriggeredMadMode_) {
        ChangeAction(ActionType::SpiralBarrage, boss);
    }

    const float deltaTimeSeconds = TimeManager::GetInstance()->GetDeltaTime();
    const float movementSpeedRate = boss.MovementSpeedRate();
    orbitAngle_ += deltaTimeSeconds * kOrbitAngularSpeed * movementSpeedRate;
    if (actionType_ != ActionType::LineMissile && actionType_ != ActionType::SpiralBarrage
        && actionType_ != ActionType::Beam) {
        missileAttackTimer_ += deltaTimeSeconds;
    }

    currentAction_->Update(*this, boss);
    if (currentAction_->IsFinished()) {
        FinishAction(boss);
    }
}

// プレイヤーを基準に移動先を決め、速度倍率を反映して頭を動かす。
void FearWormBattleState::UpdateBattleMovement(FearWormEnemy& boss, const Vector3& playerPosition, float movementSpeedRate,
    float actionElapsedSeconds, float spiralAngleRadians)
{
    const Vector3 target = BattleTarget(boss, playerPosition, actionElapsedSeconds, spiralAngleRadians);
    boss.segments_[0].position = Lerp(boss.segments_[0].position, target, boss.moveSpeed_ * movementSpeedRate);
    boss.transform_.translate = boss.segments_[0].position;
}

// プレイヤーの周囲を回り込む軌道上の目標位置を求める。
Vector3 FearWormBattleState::CalculateOrbitTarget(const FearWormMovementContext& context)
{
    Vector3 result = context.playerPosition;

    // 基本の楕円軌道を計算する。
    float orbitX = std::cos(context.orbitAngle) * kOrbitRadiusX;

    float orbitY = std::sin(context.orbitAngle) * kOrbitRadiusY;

    // 単調な周回にならないよう、横揺れと奥行きの揺れを加える。
    float lateralSway = std::sin(context.orbitAngle * kOrbitLateralSwayFrequency) * kOrbitLateralSwayAmplitude;

    float depthSway = std::sin(context.orbitAngle * kOrbitDepthSwayFrequency) * kOrbitDepthSwayAmplitude;

    result.x += orbitX + lateralSway;
    result.y += kOrbitHeight + orbitY;
    result.z += context.forwardOffset + depthSway;

    return result;
}

// 巻き付くように周回する目標位置を求める。
Vector3 FearWormBattleState::CalculateCoilTarget(const FearWormMovementContext& context)
{
    // 解除中は次の移動先へ頭を向け、胴体も通常の追従へ戻す。
    if (context.patternTime >= kCoilFormationDurationSeconds + kCoilAttackDurationSeconds) {
        return CalculateDriftTarget(context);
    }
    Vector3 result = context.playerPosition;
    float coilAngle = context.orbitAngle * kCoilAngleRate;
    result.x += std::cos(coilAngle) * kCoilBaseRadius;
    result.y += kCoilHeight;
    result.z += context.forwardOffset + std::sin(coilAngle) * kCoilBaseRadius;
    return result;
}

// 左右や上下に揺れながら進む目標位置を求める。
Vector3 FearWormBattleState::CalculateWeaveTarget(const FearWormMovementContext& context)
{
    Vector3 result = context.playerPosition;

    // 横方向と縦方向で異なる周期を使い、波打つ軌道を作る。
    float waveAngle = context.orbitAngle * kWeaveAngleRate;

    result.x += std::sin(waveAngle) * kWeaveWidth;
    result.y += kWeaveHeight + std::sin(waveAngle * kWeaveVerticalFrequency) * kWeaveVerticalAmplitude;
    result.z += context.forwardOffset + std::cos(waveAngle) * kWeaveDepthAmplitude;

    return result;
}

// ゆっくり漂う移動パターンの目標位置を求める。
Vector3 FearWormBattleState::CalculateDriftTarget(const FearWormMovementContext& context)
{
    Vector3 result = context.playerPosition;

    // 各軸を異なる周期で揺らし、緩やかに漂う軌道を作る。
    result.x += std::sin(context.orbitAngle * kDriftHorizontalFrequency) * kDriftHorizontalAmplitude;
    result.y += kDriftHeight + std::sin(context.orbitAngle * kDriftVerticalFrequency) * kDriftVerticalAmplitude;
    result.z += context.forwardOffset + std::cos(context.orbitAngle * kDriftDepthFrequency) * kDriftDepthAmplitude;

    return result;
}

// 渦を描く移動パターンの目標位置を求める。
Vector3 FearWormBattleState::CalculateSpiralTarget(const FearWormMovementContext& context)
{
    Vector3 result = context.playerPosition;
    float baseDistZ = context.forwardOffset + 50.0f;

    // 最初の2秒間は画面中央に静止して威嚇
    if (context.madModeTime < 2.0f) {
        result.x += 0.0f;
        result.y += 7.0f;
        result.z += baseDistZ;
    } else {
        // 後半の5秒間で、中心から外側へ渦を描くように広がる (半径 0.0f 〜 25.0f)
        float t = (context.madModeTime - 2.0f) / 5.0f;
        float spiralRadius = t * 25.0f;

        result.x += std::cos(context.spiralAngle) * spiralRadius;
        result.y += 7.0f + std::sin(context.spiralAngle) * spiralRadius;
        result.z += baseDistZ;
    }

    // 地面にめり込まないように下限（Y = 1.5f）をかける
    if (result.y < 1.5f) {
        result.y = 1.5f;
    }

    return result;
}

// 直線移動パターンの目標位置を求める。
Vector3 FearWormBattleState::CalculateLineTarget(const FearWormMovementContext& context)
{
    Vector3 result = context.playerPosition;
    result.x += 20.0f;
    result.y += 3.0f;
    result.z += context.forwardOffset + 75.0f;
    return result;
}

// 移動の種類に対応する軌道計算を選ぶ。
Vector3 FearWormBattleState::CalculateFearWormTarget(FearWormMovementType type, const FearWormMovementContext& context)
{
    switch (type) {
    case FearWormMovementType::Coil: return CalculateCoilTarget(context);
    case FearWormMovementType::Weave: return CalculateWeaveTarget(context);
    case FearWormMovementType::Drift: return CalculateDriftTarget(context);
    case FearWormMovementType::Line: return CalculateLineTarget(context);
    case FearWormMovementType::Spiral: return CalculateSpiralTarget(context);
    default: return CalculateOrbitTarget(context);
    }
}

// 戦闘用の軌道と経過時間をもとに、現在の追従先を計算する。
Vector3 FearWormBattleState::BattleTarget(FearWormEnemy& boss, const Vector3& playerPosition,
    float actionElapsedSeconds, float spiralAngleRadians)
{

    Vector3 adjustedPlayerPosition = playerPosition;

    // プレイヤーのカメラのピッチ角（上下の角度）を取得し、基準とするY座標を補正する
    if (boss.player_ != nullptr && boss.player_->GetCamera() != nullptr) {
        Camera* camera = boss.player_->GetCamera();

        // カメラのピッチ角を取得（ラジアン）
        // 上を向くと pitch < 0 になるため、符号を反転します
        float pitch = -camera->GetRotate().x;

        // プレイヤーとボスの奥行き（Z方向）の距離
        float distanceZ = std::abs(playerPosition.z - boss.GetPosition().z);

        // プレイヤーから見たカメラの視線の高低差（Yオフセット）を計算
        float lookOffsetY = distanceZ * std::sin(pitch);

        // ボスの基準高度を補正
        adjustedPlayerPosition.y += lookOffsetY;
    }

    return MovementTargetPosition(boss, adjustedPlayerPosition, actionElapsedSeconds, spiralAngleRadians);
}

// 現在の移動状態に応じた目標位置を取得する。
Vector3 FearWormBattleState::MovementTargetPosition(const FearWormEnemy& boss, const Vector3& playerPosition,
    float actionElapsedSeconds, float spiralAngleRadians) const
{
    const FearWormMovementContext context { playerPosition, orbitAngle_, movementPatternTimer_,
        boss.parallelForwardOffset_, actionElapsedSeconds, spiralAngleRadians };
    return CalculateFearWormTarget(movementPattern_, context);
}

// 指定した部位の位置から、通常射撃用の弾を生成する。
void FearWormBattleState::FireBullet(FearWormEnemy& boss, const Vector3& position)
{
    // プレイヤーへの発射方向を求め、共通の弾生成処理へ渡す。
    Vector3 direction = NormalizeSafe(boss.player_->GetTranslate() - position);
    FireDirectionalBullet(boss, position, direction);
}

// 頭の向きと大きさを考慮して、弾を出す位置を求める。
Vector3 FearWormBattleState::HeadMuzzlePosition(const FearWormEnemy& boss) const
{
    // 頭部中心から照準方向へ発射口分だけ前進させる。
    Vector3 position = boss.GetPosition();

    Vector3 direction = NormalizeSafe(headAimDirection_);

    position.x += direction.x * kHeadMuzzleOffset;
    position.y += direction.y * kHeadMuzzleOffset;
    position.z += direction.z * kHeadMuzzleOffset;

    return position;
}

// 頭が照準方向を向くように描画用の回転角を計算する。
Vector3 FearWormBattleState::HeadLookRotation(const FearWormEnemy& boss) const
{
    // 正規化した照準方向をピッチ角とヨー角へ変換する。
    Vector3 direction = NormalizeSafe(headAimDirection_);

    Vector3 rotate { };
    rotate.x = -std::asin(direction.y);
    rotate.y = std::atan2(direction.x, direction.z);
    rotate.z = 0.0f;

    return rotate;
}

// 指定された位置と方向を使って弾を生成する。
void FearWormBattleState::FireDirectionalBullet(FearWormEnemy& boss, const Vector3& position, const Vector3& direction,
    bool shouldPlayMuzzleEffect)
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

    boss.AddEnemyBullet(std::move(bullet));

    if (shouldPlayMuzzleEffect) {
        EffectManager::GetInstance()->PlayEffect("WormShotFlash", position);
    }
}

// ビーム本体と溜め演出を描画するリソースを準備する。
void FearWormBattleState::InitializeBeam(FearWormEnemy&)
{
    beamActionState_.Initialize();
}

// 現在のビーム状態に必要な溜め演出や光線を描画する。
void FearWormBattleState::DrawBeam(FearWormEnemy& boss)
{
    if (boss.GetBossState() == FearWormEnemy::BossState::Wait || boss.segments_.empty()
        || !boss.segments_[0].isAlive) {
        return;
    }
    beamActionState_.Draw();
}

FearWormBattleState::FearWormBattleState() = default;
FearWormBattleState::~FearWormBattleState() = default;

void FearWormBattleState::Initialize(Model* bulletModel)
{
    bulletModel_ = bulletModel;
    hasTriggeredMadMode_ = false;
}

void FearWormBattleState::Reset(FearWormEnemy& boss)
{
    Exit(boss);
    orbitShotState_ = FearWormOrbitShotState {};
    weaveSpreadState_ = FearWormWeaveSpreadState {};
    driftBurstState_ = FearWormDriftBurstState {};
    coilBarrageState_ = FearWormCoilBarrageState {};
    headChargeState_ = FearWormHeadChargeState {};
    lineMissileState_ = FearWormLineMissileState {};
    lineMissileState_.targetIndices_.reserve(boss.segments_.size());
    spiralBarrageState_ = FearWormSpiralBarrageState {};
    beamActionState_.Reset();
    movementPattern_ = MovementPattern::Orbit;
    movementPatternTimer_ = 0.0f;
    actionType_ = ActionType::OrbitShot;
    orbitAngle_ = 0.0f;
    fireSegmentIndex_ = 0;
    missileAttackTimer_ = 0.0f;
    lineTransitionTimer_ = 0.0f;
    hasTriggeredMadMode_ = false;
    headAimDirection_ = kDefaultHeadAimDirection;
}

void FearWormBattleState::PrepareEntryMovement(FearWormEnemy& boss, float orbitAngle)
{
    Exit(boss);
    orbitAngle_ = orbitAngle;
    movementPattern_ = MovementPattern::Orbit;
    movementPatternTimer_ = 0.0f;
}

void FearWormBattleState::StopAttacksForDeath(FearWormEnemy& boss)
{
    Exit(boss);
    beamActionState_.Stop();
}

float FearWormBattleState::UpdateFormationBlend()
{
    float blendT = 0.0f;
    if (movementPattern_ == MovementPattern::Line) {
        lineTransitionTimer_ += TimeManager::GetInstance()->GetDeltaTime();
        blendT = (std::min)(lineTransitionTimer_, 1.0f);
    } else {
        lineTransitionTimer_ = 0.0f;
    }
    return blendT;
}

Vector3 FearWormBattleState::CalculateFormationPosition(const FearWormEnemy& boss, int32_t segmentOrder,
    const Vector3& followPosition, float formationBlend) const
{
    if (movementPattern_ == MovementPattern::Coil) {
        const Vector3 coilPosition = CalculateCoilSegmentPosition(
            boss.segments_[0].position, orbitAngle_, segmentOrder, boss.segmentSpacing_);
        return Lerp(followPosition, coilPosition, CoilFormationBlend());
    }
    // ミサイル攻撃時の横並びと揺れを、共通の追従位置へ合成する。
    float waveScaleY = 3.2f;
    float waveScaleZ = 2.0f;
    float waveFrequency = 4.2f;

    Vector3 posLine = {
        boss.segments_[0].position.x - (boss.segmentSpacing_ * static_cast<float>(segmentOrder)),
        boss.segments_[0].position.y + std::sin(boss.moveTime_ * waveFrequency + static_cast<float>(segmentOrder) * 0.8f) * waveScaleY,
        boss.segments_[0].position.z + std::cos(boss.moveTime_ * (waveFrequency * 0.8f) + static_cast<float>(segmentOrder) * 0.6f) * waveScaleZ
    };

    // 補間率0では、通常の追従位置を維持する。
    return Lerp(followPosition, posLine, formationBlend);
}

bool FearWormBattleState::ShouldAimHead(const FearWormEnemy& boss) const
{
    bool isBeamAiming = !boss.HasAliveBodyParts() && beamActionState_.IsAiming();
    return headChargeState_.isActive_ || isBeamAiming;
}

void FearWormBattleState::SubmitRaytracing(FearWormEnemy& boss, DxrRenderer& renderer)
{
    if (!boss.segments_.empty() && boss.segments_[0].isAlive) {
        beamActionState_.SubmitRaytracing(renderer);
    }
}
float FearWormBattleState::CoilFormationBlend() const
{
    float blend = std::clamp(movementPatternTimer_ / kCoilFormationDurationSeconds, 0.0f, 1.0f);
    const float releaseStartSeconds = kCoilFormationDurationSeconds + kCoilAttackDurationSeconds;
    if (movementPatternTimer_ >= releaseStartSeconds) {
        blend = 1.0f - std::clamp(
            (movementPatternTimer_ - releaseStartSeconds) / kCoilReleaseDurationSeconds, 0.0f, 1.0f);
    }
    return blend * blend * (3.0f - 2.0f * blend);
}

bool FearWormBattleState::IsCoilFiring() const
{
    return movementPattern_ == MovementPattern::Coil
        && movementPatternTimer_ >= kCoilFormationDurationSeconds
        && movementPatternTimer_ < kCoilFormationDurationSeconds + kCoilAttackDurationSeconds;
}

Vector3 FearWormBattleState::CalculateCoilSegmentPosition(const Vector3& headPosition, float orbitAngle,
    int32_t segmentOrder, float segmentSpacing)
{
    const float headAngle = orbitAngle * kCoilAngleRate;
    const float segmentAngle = headAngle - static_cast<float>(segmentOrder) * kCoilSegmentAngleRadians;
    Vector3 position = headPosition;
    position.x += (std::cos(segmentAngle) - std::cos(headAngle)) * kCoilBaseRadius;
    position.y -= static_cast<float>(segmentOrder) * segmentSpacing * kCoilSegmentHeightRate;
    position.z += (std::sin(segmentAngle) - std::sin(headAngle)) * kCoilBaseRadius;
    return position;
}

void FearWormBattleState::Enter(FearWormEnemy& boss)
{
    ChangeAction(ActionType::OrbitShot, boss);
}

void FearWormBattleState::Exit(FearWormEnemy& boss)
{
    if (currentAction_ != nullptr) {
        currentAction_->Exit(*this, boss);
        currentAction_ = nullptr;
    }
    movementPattern_ = MovementPattern::Orbit;
    movementPatternTimer_ = 0.0f;
    lineTransitionTimer_ = 0.0f;
}

void FearWormBattleState::ChangeAction(ActionType type, FearWormEnemy& boss)
{
    if (currentAction_ != nullptr && actionType_ == type) {
        return;
    }
    FearWormBattleAction* nextAction = nullptr;
    switch (type) {
    case ActionType::OrbitShot: nextAction = &orbitShotState_; break;
    case ActionType::WeaveSpread: nextAction = &weaveSpreadState_; break;
    case ActionType::DriftBurst: nextAction = &driftBurstState_; break;
    case ActionType::CoilBarrage: nextAction = &coilBarrageState_; break;
    case ActionType::HeadCharge: nextAction = &headChargeState_; break;
    case ActionType::LineMissile: nextAction = &lineMissileState_; break;
    case ActionType::SpiralBarrage: nextAction = &spiralBarrageState_; break;
    case ActionType::Beam: nextAction = &beamActionState_; break;
    default: return;
    }
    Exit(boss);
    actionType_ = type;
    movementPatternTimer_ = 0.0f;
    currentAction_ = nextAction;
    currentAction_->Enter(*this, boss);
}

void FearWormBattleState::FinishAction(FearWormEnemy& boss)
{
    if (missileAttackTimer_ >= kMissileAttackInterval
        && actionType_ != ActionType::LineMissile && actionType_ != ActionType::SpiralBarrage) {
        ChangeAction(ActionType::LineMissile, boss);
        return;
    }
    switch (actionType_) {
    case ActionType::OrbitShot: ChangeAction(ActionType::WeaveSpread, boss); break;
    case ActionType::WeaveSpread: ChangeAction(ActionType::DriftBurst, boss); break;
    case ActionType::DriftBurst: ChangeAction(ActionType::CoilBarrage, boss); break;
    case ActionType::CoilBarrage: ChangeAction(ActionType::HeadCharge, boss); break;
    default: ChangeAction(ActionType::OrbitShot, boss); break;
    }
}

bool FearWormBattleState::CanFire(const FearWormEnemy& boss) const
{
    if (boss.GetBossState() != FearWormEnemy::BossState::Battle
        || boss.player_ == nullptr || bulletModel_ == nullptr || boss.segments_.empty()) {
        return false;
    }
    return Vector3Length(boss.player_->GetTranslate() - boss.GetPosition()) <= kAttackRange;
}

int32_t FearWormBattleState::SelectFiringSegment(FearWormEnemy& boss, int32_t& fireTimer)
{
    fireTimer++;
    if (fireTimer < fireInterval_) {
        return -1;
    }
    fireTimer = 0;
    const int32_t segmentCount = static_cast<int32_t>(boss.segments_.size());
    for (int32_t count = 0; count < segmentCount; count++) {
        fireSegmentIndex_++;
        if (fireSegmentIndex_ >= segmentCount) {
            fireSegmentIndex_ = 0;
        }
        if (boss.segments_[fireSegmentIndex_].isAlive) {
            return fireSegmentIndex_;
        }
    }
    return -1;
}

Vector4 FearWormBattleState::HeadAttackColor(const Vector4& baseColor) const
{
    return headChargeState_.HeadAttackColor(baseColor);
}

float FearWormBattleState::ApplyHeadChargePulse(float basePulse) const
{
    return headChargeState_.ApplyHeadChargePulse(basePulse);
}

// 戦闘行動の切り替えと、共通の座標・弾生成・描画処理。
#include "FearWormBattleState.h"
#include "State/FearWormTiming.h"
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
    orbitAngle_ += deltaTimeSeconds * FearWormMovement::kOrbitAngularSpeed * movementSpeedRate;
    if (actionType_ != ActionType::LineMissile && actionType_ != ActionType::SpiralBarrage
        && actionType_ != ActionType::Beam) {
        missileAttackTimer_ += deltaTimeSeconds;
    }

    currentAction_->Update(*this, boss);
    if (currentAction_->IsFinished()) {
        FinishAction(boss);
    }
    currentAction_->UpdateFormation(deltaTimeSeconds);
}

// 攻撃側で計算した目標位置へ、速度倍率と経過秒を反映して頭を動かす。
void FearWormBattleState::UpdateBattleMovement(FearWormEnemy& boss, const Vector3& target, float movementSpeedRate)
{
    const float movementBlend = FearWormTiming::CalculateFollowBlend(boss.moveSpeed_ * movementSpeedRate,
        TimeManager::GetInstance()->GetDeltaTime());
    boss.segments_[0].position = Lerp(boss.segments_[0].position, target, movementBlend);
    boss.transform_.translate = boss.segments_[0].position;
}

// 移動の種類に対応する軌道計算を選ぶ。
Vector3 FearWormBattleState::CalculateFearWormTarget(FearWormMovementType type, const FearWormMovementContext& context,
    float actionElapsedSeconds, float spiralAngleRadians)
{
    switch (type) {
    case FearWormMovementType::Coil: return FearWormCoilBarrageState::CalculateTarget(context, actionElapsedSeconds);
    case FearWormMovementType::Weave: return FearWormWeaveSpreadState::CalculateTarget(context);
    case FearWormMovementType::Drift: return FearWormMovement::CalculateDriftTarget(context);
    case FearWormMovementType::Line: return FearWormLineMissileState::CalculateTarget(context);
    case FearWormMovementType::Spiral: return FearWormSpiralBarrageState::CalculateTarget(context, actionElapsedSeconds, spiralAngleRadians);
    default: return FearWormMovement::CalculateOrbitTarget(context);
    }
}

// 視線の高さを反映した、攻撃間で共通の座標計算入力を作る。
FearWormMovementContext FearWormBattleState::CreateMovementContext(const FearWormEnemy& boss,
    const Vector3& playerPosition) const
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

    return { adjustedPlayerPosition, orbitAngle_, boss.parallelForwardOffset_ };
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
    orbitShotState_.Reset();
    weaveSpreadState_.Reset();
    driftBurstState_.Reset();
    coilBarrageState_.Reset();
    headChargeState_.Reset();
    lineMissileState_.Reset(boss.segments_.size());
    spiralBarrageState_.Reset();
    beamActionState_.Reset();
    movementPattern_ = MovementPattern::Orbit;
    actionType_ = ActionType::OrbitShot;
    orbitAngle_ = 0.0f;
    fireSegmentIndex_ = 0;
    missileAttackTimer_ = 0.0f;
    hasTriggeredMadMode_ = false;
    headAimDirection_ = kDefaultHeadAimDirection;
}

void FearWormBattleState::SetStartOrbitAngleRadians(float orbitAngleRadians)
{
    orbitAngle_ = orbitAngleRadians;
}

void FearWormBattleState::StopAttacksForDeath(FearWormEnemy& boss)
{
    Exit(boss);
    beamActionState_.Stop();
}

float FearWormBattleState::GetFormationBlend() const
{
    if (currentAction_ == nullptr) {
        return 0.0f;
    }
    return currentAction_->GetFormationBlend();
}

Vector3 FearWormBattleState::CalculateFormationPosition(const FearWormEnemy& boss, int32_t segmentOrder,
    const Vector3& followPosition, float formationBlend) const
{
    if (currentAction_ == nullptr || boss.segments_.empty()) {
        return followPosition;
    }
    const FearWormFormationContext context {
        boss.segments_[0].position, orbitAngle_, segmentOrder, boss.segmentSpacing_, boss.moveTime_
    };
    return currentAction_->CalculateFormationPosition(context, followPosition, formationBlend);
}

bool FearWormBattleState::ShouldAimHead(const FearWormEnemy& boss) const
{
    if (currentAction_ == nullptr) {
        return false;
    }
    return currentAction_->ShouldAimHead(boss);
}

void FearWormBattleState::SubmitRaytracing(FearWormEnemy& boss, DxrRenderer& renderer)
{
    if (!boss.segments_.empty() && boss.segments_[0].isAlive) {
        beamActionState_.SubmitRaytracing(renderer);
    }
}
void FearWormBattleState::Enter(FearWormEnemy& boss)
{
    ChangeAction(ActionType::OrbitShot, boss);
}

void FearWormBattleState::Exit(FearWormEnemy& boss)
{
    if (currentAction_ != nullptr) {
        // 激昂の終了・中断後は、次のミサイルまでの待機を最初から数える。
        if (actionType_ == ActionType::SpiralBarrage) {
            missileAttackTimer_ = 0.0f;
        }
        currentAction_->Exit(*this, boss);
        currentAction_ = nullptr;
    }
    movementPattern_ = MovementPattern::Orbit;
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
    // 攻撃選択に使う記録は統括側だけで更新する。
    if (type == ActionType::LineMissile) {
        missileAttackTimer_ = 0.0f;
    }
    if (type == ActionType::SpiralBarrage) {
        hasTriggeredMadMode_ = true;
    }
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

int32_t FearWormBattleState::SelectFiringSegment(FearWormEnemy& boss)
{
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
    if (currentAction_ == nullptr) {
        return baseColor;
    }
    return currentAction_->HeadAttackColor(baseColor);
}

float FearWormBattleState::ApplyHeadChargePulse(float basePulse) const
{
    if (currentAction_ == nullptr) {
        return basePulse;
    }
    return currentAction_->ApplyHeadChargePulse(basePulse);
}

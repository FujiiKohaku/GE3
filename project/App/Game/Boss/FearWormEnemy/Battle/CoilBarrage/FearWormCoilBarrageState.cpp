#include "FearWormCoilBarrageState.h"
#include "../FearWormBattleState.h"
#include "../../FearWormEnemy.h"
#include "App/Game/Player/Player.h"
#include "Engine/Time/TimeManager.h"
#include "Engine/Effect/EffectManager.h"
#include "Engine/3D/Object3d.h"
#include <cmath>
#include <algorithm>

Vector3 FearWormCoilBarrageState::CalculateFormationPosition(const FearWormFormationContext& context,
    const Vector3& followPosition, float) const
{
    return CalculateFormationPosition(context.headPosition, context.orbitAngleRadians,
        context.segmentOrder, context.segmentSpacing, followPosition);
}

void FearWormCoilBarrageState::Reset()
{
    isFinished_ = false;
    elapsedSeconds_ = 0.0f;
    nextFireSeconds_ = 0.0f;
}

void FearWormCoilBarrageState::Enter(FearWormBattleState& battle, FearWormEnemy&)
{
    Reset();
    battle.movementPattern_ = FearWormBattleState::MovementPattern::Coil;
}

void FearWormCoilBarrageState::Update(FearWormBattleState& battle, FearWormEnemy& boss)
{
    elapsedSeconds_ += TimeManager::GetInstance()->GetDeltaTime();
    const FearWormMovementContext context = battle.CreateMovementContext(boss, boss.player_->GetTranslate());
    battle.UpdateBattleMovement(boss, CalculateTarget(context, elapsedSeconds_), boss.MovementSpeedRate());
    FireBarrage(battle, boss);
    isFinished_ = elapsedSeconds_ >= kCoilFormationDurationSeconds
        + kCoilAttackDurationSeconds + kCoilReleaseDurationSeconds;
}

void FearWormCoilBarrageState::Exit(FearWormBattleState&, FearWormEnemy&)
{
    elapsedSeconds_ = 0.0f;
    nextFireSeconds_ = 0.0f;
}

void FearWormCoilBarrageState::FireBarrage(FearWormBattleState& battle, FearWormEnemy& boss)
{
    if (TimeManager::GetInstance()->GetDeltaTime() <= 0.0f) {
        return;
    }
    if (!battle.CanFire(boss) || !IsFiring(elapsedSeconds_)) {
        nextFireSeconds_ = 0.0f;
        return;
    }

    // 攻撃開始からの経過時間で、次の発射時刻を判定する。
    const float attackElapsedSeconds = elapsedSeconds_ - kCoilFormationDurationSeconds;
    if (attackElapsedSeconds < nextFireSeconds_) {
        return;
    }

    // 処理落ちからの復帰時も、同じフレームに大量の弾を生成しない。
    const float elapsedIntervals = std::floor((attackElapsedSeconds - nextFireSeconds_) / kCoilBarrageFireIntervalSeconds);
    nextFireSeconds_ += (elapsedIntervals + 1.0f) * kCoilBarrageFireIntervalSeconds;

    // 360度を弾数で均等に分割
    float angleStep = (2.0f * battle.kPi) / static_cast<float>(kCoilBarrageBulletCount);
    // 全弾の速さを維持し、広がる速さと後方へ進む速さの比率を決める。
    const float backwardSpeedRatio = 1.0f / std::sqrt(1.0f + kCoilBarrageSpreadRatio * kCoilBarrageSpreadRatio);
    const float radialSpeedRatio = kCoilBarrageSpreadRatio * backwardSpeedRatio;

    // 生存している各部位から発射
    for (size_t segmentIndex = 0; segmentIndex < boss.segments_.size(); segmentIndex++) {

        if (!boss.segments_[segmentIndex].isAlive) {
            continue;
        }

        const Vector3 position = boss.segments_[segmentIndex].position;
        const float angleOffset = static_cast<float>(segmentIndex) * kCoilBarrageSegmentAngleRadians
            + elapsedSeconds_ * kCoilBarrageAngularSpeedRadiansPerSecond;

        for (int32_t bulletIndex = 0; bulletIndex < kCoilBarrageBulletCount; bulletIndex++) {

            float angle = angleStep * static_cast<float>(bulletIndex) + angleOffset;

            // プレイヤー座標を使わず、XY平面で360度へ広げながら-Z方向へ進める。
            Vector3 direction = {
                std::cos(angle) * radialSpeedRatio,
                std::sin(angle) * radialSpeedRatio,
                -backwardSpeedRatio
            };

            battle.FireDirectionalBullet(boss, position, direction, false);
        }
        EffectManager::GetInstance()->PlayEffect("WormShotFlash", position);
    }
}

Vector3 FearWormCoilBarrageState::CalculateTarget(const FearWormMovementContext& context, float elapsedSeconds)
{
    // 解除中は次の移動先へ頭を向け、胴体も通常の追従へ戻す。
    if (elapsedSeconds >= kCoilFormationDurationSeconds + kCoilAttackDurationSeconds) {
        return FearWormMovement::CalculateDriftTarget(context);
    }
    Vector3 result = context.playerPosition;
    float coilAngle = context.orbitAngle * kCoilAngleRate;
    result.x += std::cos(coilAngle) * kCoilBaseRadius;
    result.y += kCoilHeight;
    result.z += context.forwardOffset + std::sin(coilAngle) * kCoilBaseRadius;
    return result;
}

float FearWormCoilBarrageState::CalculateFormationBlend(float elapsedSeconds)
{
    float blend = std::clamp(elapsedSeconds / kCoilFormationDurationSeconds, 0.0f, 1.0f);
    const float releaseStartSeconds = kCoilFormationDurationSeconds + kCoilAttackDurationSeconds;
    if (elapsedSeconds >= releaseStartSeconds) {
        blend = 1.0f - std::clamp(
            (elapsedSeconds - releaseStartSeconds) / kCoilReleaseDurationSeconds, 0.0f, 1.0f);
    }
    return blend * blend * (3.0f - 2.0f * blend);
}

bool FearWormCoilBarrageState::IsFiring(float elapsedSeconds)
{
    return elapsedSeconds >= kCoilFormationDurationSeconds
        && elapsedSeconds < kCoilFormationDurationSeconds + kCoilAttackDurationSeconds;
}

Vector3 FearWormCoilBarrageState::CalculateSegmentPosition(const Vector3& headPosition, float orbitAngle,
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

Vector3 FearWormCoilBarrageState::CalculateFormationPosition(const Vector3& headPosition, float orbitAngle,
    int32_t segmentOrder, float segmentSpacing, const Vector3& followPosition, float elapsedSeconds)
{
    const Vector3 coilPosition = CalculateSegmentPosition(headPosition, orbitAngle, segmentOrder, segmentSpacing);
    return Lerp(followPosition, coilPosition, CalculateFormationBlend(elapsedSeconds));
}

Vector3 FearWormCoilBarrageState::CalculateFormationPosition(const Vector3& headPosition, float orbitAngle,
    int32_t segmentOrder, float segmentSpacing, const Vector3& followPosition) const
{
    return CalculateFormationPosition(headPosition, orbitAngle, segmentOrder, segmentSpacing, followPosition, elapsedSeconds_);
}

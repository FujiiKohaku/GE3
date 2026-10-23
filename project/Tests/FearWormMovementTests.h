#pragma once
#include "App/Game/Boss/FearWormEnemy/FearWormEnemy.h"
#include <array>
#include <cmath>

inline bool IsFearWormValueClose(float firstValue, float secondValue)
{
    return std::abs(firstValue - secondValue) < 0.0001f;
}

inline bool RunFearWormMovementTests()
{
    using FearWormMovementType = FearWormEnemy::FearWormMovementType;
    using FearWormMovementContext = FearWormEnemy::FearWormMovementContext;
    FearWormMovementContext context { {10.0f, 20.0f, 30.0f}, 0.0f, 100.0f };
    const std::array<Vector3, 6> expected {{
        {26, 27, 130}, {17, 50, 130}, {10, 27, 132.5f}, {10, 28, 132}, {30, 20, 205}, {10, 27, 180}
    }};
    for (int i = 0; i < 6; ++i) {
        const Vector3 target = FearWormEnemy::CalculateFearWormTarget(static_cast<FearWormMovementType>(i), context);
        if (!IsFearWormValueClose(target.x, expected[i].x) || !IsFearWormValueClose(target.y, expected[i].y) || !IsFearWormValueClose(target.z, expected[i].z)) return false;
    }
    Vector3 target = FearWormEnemy::CalculateFearWormTarget(FearWormMovementType::Spiral, context, 7.0f);
    if (!IsFearWormValueClose(target.x, 35.0f) || !IsFearWormValueClose(target.y, 27.0f)) return false;
    context.playerPosition.y = -100.0f;
    if (FearWormEnemy::CalculateFearWormTarget(FearWormMovementType::Spiral, context).y != 1.5f) return false;
    if (FearWormEnemy::CalculateFearWormTarget(FearWormMovementType::Line, context).y != 20.0f) return false;
    context.playerPosition.y = 100.0f;
    if (FearWormEnemy::CalculateFearWormTarget(FearWormMovementType::Line, context).y != 20.0f) return false;
    // 分離前の計算結果を固定値で検証する。角度0だけでは見落とす周波数・位相も対象。
    const FearWormMovementContext angledContext { { 10.0f, 20.0f, 30.0f }, 0.7f, 100.0f };
    const std::array<Vector3, 6> angledExpected {{
        { 24.2083745f, 31.5095238f, 131.5935586f },
        { 9.4810508f, 50.0000000f, 136.9807372f },
        { 20.5364601f, 35.5453705f, 131.4643572f },
        { 13.5156985f, 30.5222894f, 131.8787454f },
        { 30.0000000f, 20.0000000f, 205.0000000f },
        { 14.5294719f, 38.6504886f, 180.0000000f }
    }};
    const std::array<float, 6> actionElapsedSeconds { 0.0f, 2.0f, 0.0f, 0.0f, 0.0f, 4.5f };
    for (size_t sampleIndex = 0; sampleIndex < angledExpected.size(); sampleIndex++) {
        const Vector3 actual = FearWormEnemy::CalculateFearWormTarget(
            static_cast<FearWormMovementType>(sampleIndex), angledContext, actionElapsedSeconds[sampleIndex], 1.2f);
        if (Vector3LengthSquared(actual - angledExpected[sampleIndex]) > 0.00000001f) {
            return false;
        }
    }
    const Vector3 releaseTarget = FearWormCoilBarrageState::CalculateTarget(angledContext, 3.5f);
    if (Vector3LengthSquared(releaseTarget - angledExpected[3]) > 0.00000001f
        || FearWormCoilBarrageState::IsFiring(1.099f)
        || !FearWormCoilBarrageState::IsFiring(1.1f)
        || !FearWormCoilBarrageState::IsFiring(3.499f)
        || FearWormCoilBarrageState::IsFiring(3.5f)
        || !IsFearWormValueClose(FearWormCoilBarrageState::CalculateFormationBlend(0.55f), 0.5f)
        || !IsFearWormValueClose(FearWormCoilBarrageState::CalculateFormationBlend(3.9f), 0.5f)) {
        return false;
    }
    const Vector3 threatTarget = { 10.0f, 27.0f, 180.0f };
    if (Vector3LengthSquared(FearWormSpiralBarrageState::CalculateTarget(angledContext, 1.999f, 1.2f) - threatTarget) > 0.00000001f
        || Vector3LengthSquared(FearWormSpiralBarrageState::CalculateTarget(angledContext, 2.0f, 1.2f) - threatTarget) > 0.00000001f) {
        return false;
    }
    // 横並びの揺れは、非ゼロ時刻・複数部位目・途中の補間率でも維持する。
    const Vector3 linePosition = FearWormLineMissileState::CalculateFormationPosition(
        { 3.0f, 8.0f, 25.0f }, 4, 5.0f, 0.37f, { 11.0f, 14.0f, -12.0f }, 0.65f);
    if (Vector3LengthSquared(linePosition - Vector3 { -7.2f, 8.0218005f, 10.9101459f }) > 0.00000001f) {
        return false;
    }
    FearWormEnemy boss;
    // 整列への補間はクラスではなく、実際の胴体更新を通して確認する。
    boss.segments_.resize(2);
    boss.segments_[0].position = {0.0f, 0.0f, 0.0f};
    boss.segments_[1].position = {0.0f, 0.0f, -5.0f};
    boss.battleState_.ChangeAction(FearWormBattleState::ActionType::LineMissile, boss);
    boss.battleState_.lineMissileState_.formationElapsedSeconds_ = 0.5f;
    boss.UpdateSegments();
    if (!IsFearWormValueClose(boss.segments_[1].position.x, -2.5f)) return false;
    boss.battleState_.lineMissileState_.formationElapsedSeconds_ = 1.5f;
    boss.UpdateSegments();
    if (!IsFearWormValueClose(boss.segments_[1].position.x, -5.0f)) return false;
    boss.battleState_.ChangeAction(FearWormBattleState::ActionType::OrbitShot, boss);
    boss.UpdateSegments();
    if (boss.battleState_.lineMissileState_.formationElapsedSeconds_ != 0.0f) { return false; }

    // 設定値の移動前と同じ時刻で、形成・発射・解除の境界を検証する。
    constexpr float kFormationDurationSeconds = 1.1f;
    constexpr float kAttackDurationSeconds = 2.4f;
    constexpr float kReleaseDurationSeconds = 0.8f;
    // とぐろは通常位置から形成し、形成後だけ射撃し、解除後に通常位置へ戻す。
    boss.battleState_.ChangeAction(FearWormBattleState::ActionType::CoilBarrage, boss);
    const Vector3 followPosition = {0.0f, 0.0f, -5.0f};
    boss.battleState_.coilBarrageState_.elapsedSeconds_ = 0.0f;
    Vector3 coilPosition = boss.battleState_.CalculateFormationPosition(boss, 1, followPosition, 0.0f);
    if (!IsFearWormValueClose(coilPosition.z, followPosition.z)
        || FearWormCoilBarrageState::IsFiring(boss.battleState_.coilBarrageState_.elapsedSeconds_)) { return false; }
    boss.battleState_.coilBarrageState_.elapsedSeconds_ = kFormationDurationSeconds;
    if (!FearWormCoilBarrageState::IsFiring(boss.battleState_.coilBarrageState_.elapsedSeconds_)
        || !IsFearWormValueClose(FearWormCoilBarrageState::CalculateFormationBlend(boss.battleState_.coilBarrageState_.elapsedSeconds_), 1.0f)) { return false; }
    Vector3 previousPosition = {7.0f, 30.0f, 0.0f};
    for (int32_t segmentOrder = 1; segmentOrder < 10; ++segmentOrder) {
        const Vector3 position = FearWormCoilBarrageState::CalculateSegmentPosition(
            {7.0f, 30.0f, 0.0f}, 0.0f, segmentOrder, 5.0f);
        const float distance = Vector3Length(position - previousPosition);
        const float radiusSquared = position.x * position.x + position.z * position.z;
        if (!IsFearWormValueClose(radiusSquared, 49.0f) || position.y >= previousPosition.y
            || distance < 4.9f || distance > 5.1f) { return false; }
        previousPosition = position;
    }
    boss.battleState_.coilBarrageState_.elapsedSeconds_ = kFormationDurationSeconds
        + kAttackDurationSeconds;
    if (FearWormCoilBarrageState::IsFiring(boss.battleState_.coilBarrageState_.elapsedSeconds_)) { return false; }
    boss.battleState_.coilBarrageState_.elapsedSeconds_ += kReleaseDurationSeconds;
    if (!IsFearWormValueClose(FearWormCoilBarrageState::CalculateFormationBlend(boss.battleState_.coilBarrageState_.elapsedSeconds_), 0.0f)) { return false; }
    boss.battleState_.ChangeAction(FearWormBattleState::ActionType::OrbitShot, boss);

    // 登場中の接触・被弾を止め、戦闘開始後に部位判定を復帰する。
    boss.ChangeState(FearWormEnemy::BossState::Entry);
    const Vector3 entryBodyPosition = boss.segments_[1].position;
    boss.UpdateSegments();
    if (boss.segments_[1].position.x != entryBodyPosition.x
        || boss.segments_[1].position.y != entryBodyPosition.y
        || boss.segments_[1].position.z != entryBodyPosition.z) { return false; }
    std::vector<EnemyCollisionPart> collisionParts;
    boss.GetCollisionParts(collisionParts);
    if (!collisionParts.empty() || boss.IsCollisionPartDamageable(1)) { return false; }
    boss.ChangeState(FearWormEnemy::BossState::Battle);
    boss.GetCollisionParts(collisionParts);
    return collisionParts.size() == 2 && boss.IsCollisionPartDamageable(1);
}

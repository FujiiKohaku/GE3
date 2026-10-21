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
    FearWormMovementContext context { {10.0f, 20.0f, 30.0f}, 0.0f, 0.0f, 100.0f, 0.0f, 0.0f };
    const std::array<Vector3, 6> expected {{
        {26, 27, 130}, {17, 50, 130}, {10, 27, 132.5f}, {10, 28, 132}, {30, 23, 205}, {10, 27, 180}
    }};
    for (int i = 0; i < 6; ++i) {
        const Vector3 target = FearWormEnemy::CalculateFearWormTarget(static_cast<FearWormMovementType>(i), context);
        if (!IsFearWormValueClose(target.x, expected[i].x) || !IsFearWormValueClose(target.y, expected[i].y) || !IsFearWormValueClose(target.z, expected[i].z)) return false;
    }
    context.madModeTime = 7.0f;
    Vector3 target = FearWormEnemy::CalculateFearWormTarget(FearWormMovementType::Spiral, context);
    if (!IsFearWormValueClose(target.x, 35.0f) || !IsFearWormValueClose(target.y, 27.0f)) return false;
    context.playerPosition.y = -100.0f;
    if (FearWormEnemy::CalculateFearWormTarget(FearWormMovementType::Spiral, context).y != 1.5f) return false;
    FearWormEnemy boss;
    // 整列への補間はクラスではなく、実際の胴体更新を通して確認する。
    boss.segments_.resize(2);
    boss.segments_[0].position = {0.0f, 0.0f, 0.0f};
    boss.segments_[1].position = {0.0f, 0.0f, -5.0f};
    boss.battleState_.ChangeAction(FearWormBattleState::ActionType::LineMissile, boss);
    boss.battleState_.lineTransitionTimer_ = 0.5f;
    boss.UpdateSegments();
    if (!IsFearWormValueClose(boss.segments_[1].position.x, -2.5f)) return false;
    boss.battleState_.lineTransitionTimer_ = 1.5f;
    boss.UpdateSegments();
    if (!IsFearWormValueClose(boss.segments_[1].position.x, -5.0f)) return false;
    boss.battleState_.ChangeAction(FearWormBattleState::ActionType::OrbitShot, boss);
    boss.UpdateSegments();
    if (boss.battleState_.lineTransitionTimer_ != 0.0f) { return false; }

    // とぐろは通常位置から形成し、形成後だけ射撃し、解除後に通常位置へ戻す。
    boss.battleState_.ChangeAction(FearWormBattleState::ActionType::CoilBarrage, boss);
    const Vector3 followPosition = {0.0f, 0.0f, -5.0f};
    boss.battleState_.movementPatternTimer_ = 0.0f;
    Vector3 coilPosition = boss.battleState_.CalculateFormationPosition(boss, 1, followPosition, 0.0f);
    if (!IsFearWormValueClose(coilPosition.z, followPosition.z)
        || boss.battleState_.IsCoilFiring()) { return false; }
    boss.battleState_.movementPatternTimer_ = FearWormBattleState::kCoilFormationDurationSeconds;
    if (!boss.battleState_.IsCoilFiring()
        || !IsFearWormValueClose(boss.battleState_.CoilFormationBlend(), 1.0f)) { return false; }
    Vector3 previousPosition = {7.0f, 30.0f, 0.0f};
    for (int32_t segmentOrder = 1; segmentOrder < 10; ++segmentOrder) {
        const Vector3 position = FearWormBattleState::CalculateCoilSegmentPosition(
            {7.0f, 30.0f, 0.0f}, 0.0f, segmentOrder, 5.0f);
        const float distance = Vector3Length(position - previousPosition);
        const float radiusSquared = position.x * position.x + position.z * position.z;
        if (!IsFearWormValueClose(radiusSquared, 49.0f) || position.y >= previousPosition.y
            || distance < 4.9f || distance > 5.1f) { return false; }
        previousPosition = position;
    }
    boss.battleState_.movementPatternTimer_ = FearWormBattleState::kCoilFormationDurationSeconds
        + FearWormBattleState::kCoilAttackDurationSeconds;
    if (boss.battleState_.IsCoilFiring()) { return false; }
    boss.battleState_.movementPatternTimer_ += FearWormBattleState::kCoilReleaseDurationSeconds;
    if (!IsFearWormValueClose(boss.battleState_.CoilFormationBlend(), 0.0f)) { return false; }
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

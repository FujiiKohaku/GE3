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
        {26, 27, 130}, {18, 28, 130}, {10, 27, 132.5f}, {10, 28, 132}, {30, 23, 205}, {10, 27, 180}
    }};
    for (int i = 0; i < 6; ++i) {
        const Vector3 target = FearWormEnemy::CalculateFearWormTarget(static_cast<FearWormMovementType>(i), context);
        if (!IsFearWormValueClose(target.x, expected[i].x) || !IsFearWormValueClose(target.y, expected[i].y) || !IsFearWormValueClose(target.z, expected[i].z)) return false;
    }
    auto type = FearWormMovementType::Orbit;
    for (auto next : {FearWormMovementType::Coil, FearWormMovementType::Weave,
        FearWormMovementType::Drift, FearWormMovementType::Orbit}) {
        if (FearWormEnemy::NextFearWormMovement(type) != next) return false;
        type = next;
    }
    if (FearWormEnemy::NextFearWormMovement(FearWormMovementType::Line) != FearWormMovementType::Line ||
        FearWormEnemy::NextFearWormMovement(FearWormMovementType::Spiral) != FearWormMovementType::Spiral) return false;
    context.madModeTime = 7.0f;
    Vector3 target = FearWormEnemy::CalculateFearWormTarget(FearWormMovementType::Spiral, context);
    if (!IsFearWormValueClose(target.x, 35.0f) || !IsFearWormValueClose(target.y, 27.0f)) return false;
    context.playerPosition.y = -100.0f;
    if (FearWormEnemy::CalculateFearWormTarget(FearWormMovementType::Spiral, context).y != 1.5f) return false;
    FearWormEnemy boss;
    boss.movementPatternTimer_ = 3.0f;
    boss.lineTransitionTimer_ = 5.0f;
    boss.ChangeMovementPattern(FearWormMovementType::Line);
    if (boss.movementPattern_ != FearWormMovementType::Line || boss.movementPatternTimer_ != 3.0f ||
        boss.lineTransitionTimer_ != 0.0f) return false;
    boss.ChangeMovementPattern(FearWormMovementType::Spiral);
    if (boss.movementPatternTimer_ != 3.0f) return false;
    boss.ChangeMovementPattern(FearWormMovementType::Orbit);
    if (boss.movementPatternTimer_ != 0.0f) return false;

    // 整列への補間はクラスではなく、実際の胴体更新を通して確認する。
    boss.segments_.resize(2);
    boss.segments_[0].position = {0.0f, 0.0f, 0.0f};
    boss.segments_[1].position = {0.0f, 0.0f, -5.0f};
    boss.ChangeMovementPattern(FearWormMovementType::Line);
    boss.lineTransitionTimer_ = 0.5f;
    boss.UpdateSegments();
    if (!IsFearWormValueClose(boss.segments_[1].position.x, -2.5f)) return false;
    boss.lineTransitionTimer_ = 1.5f;
    boss.UpdateSegments();
    if (!IsFearWormValueClose(boss.segments_[1].position.x, -5.0f)) return false;
    boss.ChangeMovementPattern(FearWormMovementType::Orbit);
    boss.UpdateSegments();
    return boss.lineTransitionTimer_ == 0.0f;
}

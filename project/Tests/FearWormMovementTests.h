#pragma once
#include "App/Game/Boss/FearWormEnemy/FearWormEnemy.h"
#include <array>
#include <cmath>

inline bool RunFearWormMovementTests()
{
    const auto close = [](float a, float b) { return std::abs(a - b) < 0.0001f; };
    FearWormMovementContext context { {10.0f, 20.0f, 30.0f}, 0.0f, 0.0f, 100.0f, 0.0f, 0.0f };
    const std::array<Vector3, 6> expected {{
        {26, 27, 130}, {18, 28, 130}, {10, 27, 132.5f}, {10, 28, 132}, {30, 23, 205}, {10, 27, 180}
    }};
    for (int i = 0; i < 6; ++i) {
        const BaseFearWormMovement& movement = GetFearWormMovement(static_cast<FearWormMovementType>(i));
        const Vector3 target = movement.CalculateTarget(context);
        if (!close(target.x, expected[i].x) || !close(target.y, expected[i].y) || !close(target.z, expected[i].z)) return false;
    }
    auto type = FearWormMovementType::Orbit;
    for (auto next : {FearWormMovementType::Coil, FearWormMovementType::Weave,
        FearWormMovementType::Drift, FearWormMovementType::Orbit}) {
        const auto& movement = GetFearWormMovement(type);
        if (!movement.AdvancesAutomatically() || movement.GetNext() != next) return false;
        type = movement.GetNext();
    }
    const auto& line = GetFearWormMovement(FearWormMovementType::Line);
    const auto& spiral = GetFearWormMovement(FearWormMovementType::Spiral);
    if (line.AdvancesAutomatically() || spiral.AdvancesAutomatically()) return false;
    float patternTime = 3.0f, lineTime = 5.0f;
    line.Enter({patternTime, lineTime});
    if (patternTime != 3.0f || lineTime != 0.0f ||
        !close(line.UpdateLineBlend({patternTime, lineTime}, 0.5f), 0.5f) ||
        line.UpdateLineBlend({patternTime, lineTime}, 1.0f) != 1.0f) return false;
    GetFearWormMovement(FearWormMovementType::Orbit).UpdateLineBlend({patternTime, lineTime}, 1.0f);
    if (lineTime != 0.0f) return false;
    context.madModeTime = 7.0f;
    Vector3 target = spiral.CalculateTarget(context);
    if (!close(target.x, 35.0f) || !close(target.y, 27.0f)) return false;
    context.playerPosition.y = -100.0f;
    if (spiral.CalculateTarget(context).y != 1.5f) return false;
    FearWormEnemy boss;
    boss.movementPatternTimer_ = 3.0f;
    boss.lineTransitionTimer_ = 5.0f;
    boss.ChangeMovementPattern(FearWormMovementType::Line);
    if (boss.movement_->GetType() != FearWormMovementType::Line || boss.movementPatternTimer_ != 3.0f ||
        boss.lineTransitionTimer_ != 0.0f) return false;
    boss.ChangeMovementPattern(FearWormMovementType::Spiral);
    if (boss.movementPatternTimer_ != 3.0f) return false;
    boss.ChangeMovementPattern(FearWormMovementType::Orbit);
    return boss.movementPatternTimer_ == 0.0f;
}

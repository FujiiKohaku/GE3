#pragma once
#include "Engine/math/MathStruct.h"

enum class FearWormMovementType { Orbit, Coil, Weave, Drift, Line, Spiral };

struct FearWormMovementContext {
    Vector3 playerPosition;
    float orbitAngle;
    float patternTime;
    float forwardOffset;
    float madModeTime;
    float spiralAngle;
};

// 状態を持たない移動先計算。タイマーの管理はFearWormEnemyが行う。
Vector3 CalculateFearWormTarget(FearWormMovementType type, const FearWormMovementContext& context);
FearWormMovementType NextFearWormMovement(FearWormMovementType type);

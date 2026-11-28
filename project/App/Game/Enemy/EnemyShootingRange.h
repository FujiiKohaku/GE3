#pragma once

#include "Engine/math/MathStruct.h"

inline constexpr float kEnemyShootingRange = 280.0f;

inline bool IsEnemyAhead(const Vector3& fromPlayerToEnemy, const Vector3& playerFlightForward)
{
    return Dot(fromPlayerToEnemy, playerFlightForward) > 0.0f;
}

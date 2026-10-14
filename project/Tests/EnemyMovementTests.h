#pragma once
#include "App/Game/Enemy/MoveEnemy/MoveEnemy.h"
#include <cmath>
#include <numbers>

inline bool RunEnemyMovementTests()
{
    const auto close = [](float a, float b) { return std::abs(a - b) < 0.0001f; };
    EnemyMovementContext context { { 10.0f, 20.0f, 30.0f }, 0.0f, 1.0f, 4.0f, 2.0f };
    for (int i = 0; i < 4; ++i) {
        const BaseEnemyMovement* movement = &GetEnemyMovement(static_cast<MovePattern>(i));
        const Vector3 position = movement->CalculatePosition(context);
        if (!close(position.x, 10.0f) || !close(position.y, 20.0f) || !close(position.z, 30.0f)) return false;
    }
    context.time = std::numbers::pi_v<float> / 2.0f;
    auto position = GetEnemyMovement(MovePattern::LeftRight).CalculatePosition(context);
    if (!close(position.x, 14.0f) || !close(position.y, 20.0f) || !close(position.z, 30.0f)) return false;
    position = GetEnemyMovement(MovePattern::UpDown).CalculatePosition(context);
    if (!close(position.x, 10.0f) || !close(position.y, 24.0f)) return false;
    context.time = std::numbers::pi_v<float> / 4.0f;
    position = GetEnemyMovement(MovePattern::ZigZag).CalculatePosition(context);
    if (!close(position.x, 12.828427f) || !close(position.y, 22.0f)) return false;
    position = GetEnemyMovement(MovePattern::SineWave).CalculatePosition(context);
    if (!close(position.x, 10.0f) || !close(position.y, 24.0f) || !close(position.z, 28.821903f)) return false;
    // 同じ時間・開始位置から計算するため、呼び出す回数で座標は変わらない。
    const Vector3 again = GetEnemyMovement(MovePattern::SineWave).CalculatePosition(context);
    if (position.x != again.x || position.y != again.y || position.z != again.z) return false;
    context.amplitude = 0.0f;
    position = GetEnemyMovement(MovePattern::SineWave).CalculatePosition(context);
    if (!close(position.y, 20.0f) || !close(position.z, 28.821903f)) return false;
    MoveEnemy enemy;
    enemy.SetMovePattern(MovePattern::ZigZag);
    enemy.SetMovePattern(static_cast<MovePattern>(99));
    return enemy.GetMovePattern() == MovePattern::ZigZag;
}

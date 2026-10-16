#pragma once
#include "App/Game/Enemy/MoveEnemy/MoveEnemy.h"
#include <cmath>
#include <numbers>

inline bool IsEnemyMovementValueClose(float firstValue, float secondValue)
{
    return std::abs(firstValue - secondValue) < 0.0001f;
}

inline bool RunEnemyMovementCalculationTests()
{
    const Vector3 startPosition { 10.0f, 20.0f, 30.0f };
    constexpr float kSpeed = 1.0f;
    constexpr float kAmplitude = 4.0f;
    constexpr float kFrequency = 2.0f;
    for (int patternIndex = 0; patternIndex < 4; ++patternIndex) {
        const Vector3 position = MoveEnemy::CalculateMovementPosition(
            static_cast<MovePattern>(patternIndex), startPosition, 0.0f, kSpeed, kAmplitude, kFrequency);
        if (!IsEnemyMovementValueClose(position.x, 10.0f) || !IsEnemyMovementValueClose(position.y, 20.0f)
            || !IsEnemyMovementValueClose(position.z, 30.0f)) { return false; }
    }
    float elapsedSeconds = std::numbers::pi_v<float> / 2.0f;
    Vector3 position = MoveEnemy::CalculateMovementPosition(
        MovePattern::LeftRight, startPosition, elapsedSeconds, kSpeed, kAmplitude, kFrequency);
    if (!IsEnemyMovementValueClose(position.x, 14.0f) || !IsEnemyMovementValueClose(position.y, 20.0f)
        || !IsEnemyMovementValueClose(position.z, 30.0f)) { return false; }
    position = MoveEnemy::CalculateMovementPosition(
        MovePattern::UpDown, startPosition, elapsedSeconds, kSpeed, kAmplitude, kFrequency);
    if (!IsEnemyMovementValueClose(position.x, 10.0f) || !IsEnemyMovementValueClose(position.y, 24.0f)) { return false; }
    elapsedSeconds = std::numbers::pi_v<float> / 4.0f;
    position = MoveEnemy::CalculateMovementPosition(
        MovePattern::ZigZag, startPosition, elapsedSeconds, kSpeed, kAmplitude, kFrequency);
    if (!IsEnemyMovementValueClose(position.x, 12.828427f) || !IsEnemyMovementValueClose(position.y, 22.0f)) { return false; }
    position = MoveEnemy::CalculateMovementPosition(
        MovePattern::SineWave, startPosition, elapsedSeconds, kSpeed, kAmplitude, kFrequency);
    if (!IsEnemyMovementValueClose(position.x, 10.0f) || !IsEnemyMovementValueClose(position.y, 24.0f)
        || !IsEnemyMovementValueClose(position.z, 28.821903f)) { return false; }
    // 同じ時間・開始位置から計算するため、呼び出す回数で座標は変わらない。
    const Vector3 again = MoveEnemy::CalculateMovementPosition(
        MovePattern::SineWave, startPosition, elapsedSeconds, kSpeed, kAmplitude, kFrequency);
    if (position.x != again.x || position.y != again.y || position.z != again.z) { return false; }
    position = MoveEnemy::CalculateMovementPosition(
        MovePattern::SineWave, startPosition, elapsedSeconds, kSpeed, 0.0f, kFrequency);
    return IsEnemyMovementValueClose(position.y, 20.0f) && IsEnemyMovementValueClose(position.z, 28.821903f);
}

inline bool RunEnemyMovementTests()
{
    if (!RunEnemyMovementCalculationTests()) { return false; }
    MoveEnemy enemy;
    enemy.SetMovePattern(MovePattern::ZigZag);
    enemy.SetMovePattern(static_cast<MovePattern>(99));
    return enemy.GetMovePattern() == MovePattern::ZigZag;
}

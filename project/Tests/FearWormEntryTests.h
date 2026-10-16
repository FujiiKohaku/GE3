#pragma once
#include "App/Game/Boss/FearWormEnemy/FearWormEnemy.h"
#include <cmath>
#include <cstdio>

inline bool RunFearWormEntryTests()
{
    constexpr size_t kSegmentCount = 10;
    constexpr float kSpacing = 5.0f;
    const Vector3 battlePosition = { -11.2f, 12.0f, 5200.0f };
    const float durationSeconds = FearWormEnemy::CalculateEntryDurationSeconds(kSegmentCount);
    for (float waterHeight : { -30.0f, 15.0f }) {
        const Vector3 surfacePosition = { -12.0f, waterHeight, 5234.0f };
        for (size_t index = 0; index < kSegmentCount; ++index) {
            const Vector3 waitingPosition = FearWormEnemy::CalculateEntryPosition(surfacePosition,
                battlePosition, FearWormEnemy::kEntryShadowDurationSeconds, index, kSpacing);
            if (waitingPosition.y >= waterHeight) { return false; }
            const Vector3 finalPosition = FearWormEnemy::CalculateEntryPosition(surfacePosition,
                battlePosition, durationSeconds, index, kSpacing);
            if (std::abs(finalPosition.x - battlePosition.x) > 0.001f
                || std::abs(finalPosition.y - battlePosition.y) > 0.001f
                || std::abs(finalPosition.z - (battlePosition.z - index * kSpacing)) > 0.001f) { return false; }
        }
        // 低フレームレートでも、頭から順番に水面を越えることを確認する。
        for (float deltaSeconds : { 1.0f / 30.0f, 1.0f / 60.0f, 1.0f / 144.0f }) {
            float previousCrossingSeconds = -1.0f;
            for (size_t index = 0; index < kSegmentCount; ++index) {
                bool hasCrossedSurface = false;
                Vector3 previousPosition = FearWormEnemy::CalculateEntryPosition(surfacePosition, battlePosition, 0.0f, index, kSpacing);
                for (float elapsedSeconds = deltaSeconds; elapsedSeconds < durationSeconds; elapsedSeconds += deltaSeconds) {
                    const Vector3 position = FearWormEnemy::CalculateEntryPosition(surfacePosition, battlePosition, elapsedSeconds, index, kSpacing);
                    if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z)
                        || std::abs(position.x - previousPosition.x) > 8.0f
                        || std::abs(position.y - previousPosition.y) > 8.0f
                        || std::abs(position.z - previousPosition.z) > 8.0f) { return false; }
                    if (!hasCrossedSurface && position.y >= waterHeight) {
                        if (elapsedSeconds <= previousCrossingSeconds) { return false; }
                        previousCrossingSeconds = elapsedSeconds;
                        hasCrossedSurface = true;
                    }
                    previousPosition = position;
                }
                if (!hasCrossedSurface) { return false; }
            }
        }
    }
    std::puts("FearWorm entry: delayed breach, continuous motion, water heights and battle handoff passed.");
    return true;
}

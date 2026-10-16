#pragma once
#include "App/Game/Enemy/SwarmEnemy/SwarmEnemy.h"
#include <array>
#include <cmath>

inline bool IsSwarmFormationValueClose(float firstValue, float secondValue)
{
    return std::abs(firstValue - secondValue) < 0.0001f;
}

inline bool RunSwarmFormationTests()
{
    const std::array<Vector3, 6> firstPositions {{
        {4.5f, 0.0f, 0.0f}, {-6.25f, -6.0f, 0.0f}, {-9.0f, 8.0f, 1.3f},
        {-1.3f, 8.0f, 0.0f}, {-12.325f, 0.0f, 2.2f}, {-10.0f, 8.0f, 1.1f}
    }};
    for (int i = 0; i < 6; ++i) {
        const SwarmFormationType type = static_cast<SwarmFormationType>(i);
        const Vector3 first = SwarmEnemy::CalculateFormationOffset(type, {0, 1, 0.0f});
        const Vector3 expected = firstPositions[i];
        if (!IsSwarmFormationValueClose(first.x, expected.x) || !IsSwarmFormationValueClose(first.y, expected.y) || !IsSwarmFormationValueClose(first.z, expected.z)) return false;
        // 全18枠と両方向で、時間が進んでも配置計算が有効で再現可能なこと。
        for (int slot = 0; slot < 18; ++slot) {
            for (int direction : {-1, 1}) {
                for (float time : {0.0f, 0.5f, 10.0f}) {
                    const SwarmEnemy::FormationContext context {slot, direction, time};
                    const Vector3 offset = SwarmEnemy::CalculateFormationOffset(type, context);
                    const Vector3 again = SwarmEnemy::CalculateFormationOffset(type, context);
                    if (!std::isfinite(offset.x) || !std::isfinite(offset.y) || !std::isfinite(offset.z) ||
                        offset.x != again.x || offset.y != again.y || offset.z != again.z) return false;
                }
            }
        }
    }
    if (!IsSwarmFormationValueClose(SwarmEnemy::CalculateFormationOffset(SwarmFormationType::Wall, {5, 1, 0.0f}).x, 6.25f) ||
        !IsSwarmFormationValueClose(SwarmEnemy::CalculateFormationOffset(SwarmFormationType::Wall, {6, 1, 0.0f}).y, 0.0f)) return false;
    if (!IsSwarmFormationValueClose(SwarmEnemy::CalculateFormationOffset(SwarmFormationType::Diamond, {6, 1, 0.0f}).x, -6.5f) ||
        !IsSwarmFormationValueClose(SwarmEnemy::CalculateFormationOffset(SwarmFormationType::Diamond, {16, 1, 0.0f}).y, -8.0f)) return false;
    if (!IsSwarmFormationValueClose(SwarmEnemy::CalculateFormationOffset(SwarmFormationType::Arrow, {0, -1, 0.0f}).x, 10.0f) ||
        !IsSwarmFormationValueClose(SwarmEnemy::CalculateFormationOffset(SwarmFormationType::Arrow, {17, 1, 0.0f}).x, 3.0f)) return false;
    const Vector4 glyphColor = SwarmEnemy::GetFormationColor(SwarmFormationType::Glyph);
    const Vector4 arrowColor = SwarmEnemy::GetFormationColor(SwarmFormationType::Arrow);
    const SwarmEnemy::FormationContext context { 4, -1, 0.5f };
    const Vector3 fallback = SwarmEnemy::CalculateFormationOffset(static_cast<SwarmFormationType>(99), context);
    const Vector3 spiral = SwarmEnemy::CalculateFormationOffset(SwarmFormationType::Spiral, context);
    const Vector4 fallbackColor = SwarmEnemy::GetFormationColor(static_cast<SwarmFormationType>(99));
    const Vector4 spiralColor = SwarmEnemy::GetFormationColor(SwarmFormationType::Spiral);
    return glyphColor.x == arrowColor.x && glyphColor.y == arrowColor.y
        && glyphColor.z == arrowColor.z && glyphColor.w == arrowColor.w
        && fallback.x == spiral.x && fallback.y == spiral.y && fallback.z == spiral.z
        && fallbackColor.x == spiralColor.x && fallbackColor.y == spiralColor.y
        && fallbackColor.z == spiralColor.z && fallbackColor.w == spiralColor.w;
}

#pragma once
#include "App/Game/Enemy/SwarmEnemy/BaseSwarmFormation.h"
#include <array>
#include <cmath>

inline bool RunSwarmFormationTests()
{
    const auto close = [](float a, float b) { return std::abs(a - b) < 0.0001f; };
    const std::array<Vector3, 6> firstPositions {{
        {4.5f, 0.0f, 0.0f}, {-6.25f, -6.0f, 0.0f}, {-9.0f, 8.0f, 1.3f},
        {-1.3f, 8.0f, 0.0f}, {-12.325f, 0.0f, 2.2f}, {-10.0f, 8.0f, 1.1f}
    }};
    for (int i = 0; i < 6; ++i) {
        const auto& definition = GetSwarmFormation(static_cast<SwarmFormationType>(i));
        const BaseSwarmFormation* formation = definition.strategy;
        if (!formation) return false;
        const Vector3 first = formation->CalculateOffset({0, 1, 0.0f});
        const Vector3 expected = firstPositions[i];
        if (!close(first.x, expected.x) || !close(first.y, expected.y) || !close(first.z, expected.z)) return false;
        // 全18枠と両方向で、時間が進んでも配置計算が有効で再現可能なこと。
        for (int slot = 0; slot < 18; ++slot) {
            for (int direction : {-1, 1}) {
                for (float time : {0.0f, 0.5f, 10.0f}) {
                    const SwarmFormationContext context {slot, direction, time};
                    const Vector3 offset = formation->CalculateOffset(context);
                    const Vector3 again = formation->CalculateOffset(context);
                    if (!std::isfinite(offset.x) || !std::isfinite(offset.y) || !std::isfinite(offset.z) ||
                        offset.x != again.x || offset.y != again.y || offset.z != again.z) return false;
                }
            }
        }
    }
    const auto* wall = GetSwarmFormation(SwarmFormationType::Wall).strategy;
    if (!close(wall->CalculateOffset({5, 1, 0.0f}).x, 6.25f) ||
        !close(wall->CalculateOffset({6, 1, 0.0f}).y, 0.0f)) return false;
    const auto* diamond = GetSwarmFormation(SwarmFormationType::Diamond).strategy;
    if (!close(diamond->CalculateOffset({6, 1, 0.0f}).x, -6.5f) ||
        !close(diamond->CalculateOffset({16, 1, 0.0f}).y, -8.0f)) return false;
    const auto* arrow = GetSwarmFormation(SwarmFormationType::Arrow).strategy;
    if (!close(arrow->CalculateOffset({0, -1, 0.0f}).x, 10.0f) ||
        !close(arrow->CalculateOffset({17, 1, 0.0f}).x, 3.0f)) return false;
    const Vector4 glyphColor = GetSwarmFormation(SwarmFormationType::Glyph).color;
    const Vector4 arrowColor = GetSwarmFormation(SwarmFormationType::Arrow).color;
    return glyphColor.x == arrowColor.x && glyphColor.y == arrowColor.y &&
        GetSwarmFormation(static_cast<SwarmFormationType>(99)).strategy ==
        GetSwarmFormation(SwarmFormationType::Spiral).strategy;
}

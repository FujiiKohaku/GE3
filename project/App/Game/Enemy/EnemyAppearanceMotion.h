// 敵の目・脚・尾などのパーツを、視線や時間に応じて平行移動させる計算をまとめる。
#pragma once
#include "Engine/math/MathStruct.h"
#include <cmath>
#include <algorithm>

enum class EnemyAppearancePart { Eye, Left, Right, Tail, Cannon, Tank };

inline constexpr float kEnemyAppearanceBodyHover = 0.035f;

// 敵から見たプレイヤーの位置を、目が動ける範囲内の移動量に変換する。
inline Vector3 CalculateEnemyEyeTranslate(const Vector3& localTarget, const Vector3& travelRange)
{
    const float forwardDistance = (std::max)(std::abs(localTarget.z), 1.0f);
    constexpr float kGazeGain = 8.0f;
    return { std::clamp(localTarget.x * kGazeGain / forwardDistance, -1.0f, 1.0f) * travelRange.x,
        std::clamp(localTarget.y * kGazeGain / forwardDistance, -1.0f, 1.0f) * travelRange.y, 0 };
}

// 追加パーツが本体から離れすぎないよう、移動量の長さを制限する。
inline Vector3 LimitEnemyPartTranslate(const Vector3& offset, float maxDistance)
{
    const float distance = std::sqrt(offset.x * offset.x + offset.y * offset.y + offset.z * offset.z);
    if (distance <= maxDistance || distance <= 0.000001f) { return offset; }
    const float factor = (std::max)(maxDistance, 0.0f) / distance;
    return { offset.x * factor, offset.y * factor, offset.z * factor };
}

// 目・脚・尾・砲口などの種類ごとに、視線・揺れ・反動の移動量を求める。
inline Vector3 CalculateEnemyPartTranslate(EnemyAppearancePart part, float timeSeconds,
    float phaseRadians, const Vector3& eyeOffset, float recoilSeconds)
{
    Vector3 offset {};
    const float wave = std::sin(timeSeconds * 3.5f + phaseRadians);
    if (part == EnemyAppearancePart::Eye) { return eyeOffset; }
    if (part == EnemyAppearancePart::Left) {
        offset.x = 0.09f * wave;
        offset.y = 0.14f * wave;
    }
    if (part == EnemyAppearancePart::Right) {
        offset.x = -0.09f * wave;
        offset.y = -0.14f * wave;
    }
    if (part == EnemyAppearancePart::Tail) {
        offset.y = 0.18f * std::sin(timeSeconds * 7.0f + phaseRadians);
    }
    if (part == EnemyAppearancePart::Cannon) {
        offset.z = 0.11f * std::clamp(recoilSeconds / 0.16f, 0.0f, 1.0f);
    }
    if (part == EnemyAppearancePart::Tank) {
        offset.y = 0.07f * std::sin(timeSeconds * 2.4f + phaseRadians);
    }
    return offset;
}

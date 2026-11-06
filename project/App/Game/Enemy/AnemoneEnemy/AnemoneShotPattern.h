#pragma once
#include "Engine/math/MathStruct.h"
#include <array>
#include <cmath>
#include <limits>

namespace AnemoneShotPattern {
inline constexpr int kBulletCount = 5;
inline constexpr float kBulletSpeed = 0.55f; // 60FPS基準。BaseBulletが経過秒へ変換する。
inline constexpr float kFireIntervalSeconds = 2.4f;
inline constexpr float kAttackRange = 180.0f;

inline bool IsTargetInRange(const Vector3& muzzle, const Vector3& target,
    const Vector3& targetForward, float waterHeight)
{
    const Vector3 offset = muzzle - target;
    return target.y < waterHeight - 5.0f && Dot(offset, targetForward) > 0.0f &&
        Vector3Length(offset) <= kAttackRange && Vector3Length(offset) > 0.001f;
}

// 自動前進を先読みし、中央の弾がプレイヤーと交差する方向を求める。
inline std::array<Vector3, kBulletCount> CalculateDirections(const Vector3& muzzle,
    const Vector3& target, const Vector3& targetVelocity)
{
    const Vector3 offset = target - muzzle;
    const float a = Dot(targetVelocity, targetVelocity) - kBulletSpeed * kBulletSpeed;
    const float b = 2.0f * Dot(offset, targetVelocity);
    const float c = Dot(offset, offset);
    float frames = 0.0f;
    if (std::abs(a) < 0.00001f) {
        if (b < -0.00001f) { frames = -c / b; }
    } else {
        const float discriminant = b * b - 4.0f * a * c;
        if (discriminant >= 0.0f) {
            const float root = std::sqrt(discriminant);
            const float first = (-b - root) / (2.0f * a);
            const float second = (-b + root) / (2.0f * a);
            float earliest = (std::numeric_limits<float>::max)();
            if (first > 0.0f) { earliest = first; }
            if (second > 0.0f && second < earliest) { earliest = second; }
            if (earliest != (std::numeric_limits<float>::max)()) { frames = earliest; }
        }
    }
    const Vector3 aimOffset = offset + targetVelocity * frames;
    const Vector3 aim = Vector3Length(aimOffset) > 0.001f ? Normalize(aimOffset) : Vector3 { 0, 0, -1 };
    Vector3 right = Cross(Vector3 { 0, 1, 0 }, aim);
    if (Vector3Length(right) < 0.001f) { right = { 1, 0, 0 }; }
    right = Normalize(right);
    std::array<Vector3, kBulletCount> directions;
    constexpr float kStepRadians = 0.12217305f; // 7度ずつ、全体28度の扇。
    for (int index = 0; index < kBulletCount; ++index) {
        const float angle = static_cast<float>(index - kBulletCount / 2) * kStepRadians;
        directions[index] = Normalize(aim * std::cos(angle) + right * std::sin(angle));
    }
    return directions;
}
}

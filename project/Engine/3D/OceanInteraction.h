#pragma once
#include "Engine/Math/MathStruct.h"
#include <algorithm>
#include <cmath>

namespace OceanInteraction {
inline float FollowWakeValue(float current, float target, float deltaTimeSeconds) {
    if (deltaTimeSeconds <= 0.0f) {
        return current;
    }
    constexpr float kFollowTimeSeconds = 0.15f;
    return current + (target - current) * (1.0f - std::exp(-deltaTimeSeconds / kFollowTimeSeconds));
}

inline bool ShouldEmitSphereImpact(bool& isArmed, float previousOffset, float currentOffset, float radius) {
    constexpr float kRearmMargin = 0.5f;
    if (radius <= 0.0f) {
        isArmed = false;
        return false;
    }
    if (std::abs(previousOffset) > radius + kRearmMargin || std::abs(currentOffset) > radius + kRearmMargin) {
        isArmed = true;
    }
    const bool hasCrossed = (previousOffset < 0.0f && currentOffset >= 0.0f)
        || (previousOffset > 0.0f && currentOffset <= 0.0f);
    if (isArmed && hasCrossed) {
        isArmed = false;
        return true;
    }
    return false;
}

// Includes a complete crossing between frames, while rejecting fully submerged motion.
inline float FindSphereContactFraction(float previousOffset, float currentOffset, float radius) {
    if (radius <= 0.0f) {
        return -1.0f;
    }
    if (std::abs(currentOffset) <= radius) {
        return 1.0f;
    }
    if (previousOffset * currentOffset < 0.0f) {
        return std::clamp(previousOffset / (previousOffset - currentOffset), 0.0f, 1.0f);
    }
    return -1.0f;
}

#define OCEAN_WAKE_VECTOR Vector3
#define OCEAN_WAKE_MAKE(x, y, z) Vector3{x, y, z}
#define OCEAN_WAKE_EXP(x) std::exp(x)
#define OCEAN_WAKE_SQRT(x) std::sqrt(x)
#define OCEAN_WAKE_ABS(x) std::abs(x)
#define OCEAN_WAKE_CLAMP(x, a, b) std::clamp(x, a, b)
#define OCEAN_WAKE_INLINE inline
#include "../../resources/Shaders/Ocean/OceanWakeShape.hlsli"

inline Vector3 HorizontalDirection(const Vector3& direction) {
    const float length = std::sqrt(direction.x * direction.x + direction.z * direction.z);
    return length > 0.001f ? Vector3{direction.x / length, 0, direction.z / length} : Vector3{0, 0, 1};
}

// Smooth yaw along the shortest arc; preserve heading at rest and bound sudden reversals.
inline Vector3 FollowWakeDirection(const Vector3& current, const Vector3& velocity, float deltaTime) {
    const auto heading = HorizontalDirection(current);
    if (deltaTime <= 0 || velocity.x * velocity.x + velocity.z * velocity.z < 0.25f) return heading;
    const auto target = HorizontalDirection(velocity);
    const float angle = std::atan2(heading.z * target.x - heading.x * target.z,
        heading.x * target.x + heading.z * target.z);
    const float follow = 1.0f - std::exp(-deltaTime / 0.18f);
    const float step = std::clamp(angle * follow, -2.5f * deltaTime, 2.5f * deltaTime);
    const float sine = std::sin(step), cosine = std::cos(step);
    return {heading.x * cosine + heading.z * sine, 0, heading.z * cosine - heading.x * sine};
}

inline Vector3 SourcePosition(const Vector3& position, const Vector3& forward, float speed) {
    const float horizontalLength = std::sqrt(forward.x * forward.x + forward.z * forward.z);
    const float fx = horizontalLength > 0.001f ? forward.x / horizontalLength : 0.0f;
    const float fz = horizontalLength > 0.001f ? forward.z / horizontalLength : 1.0f;
    const float lead = std::clamp(speed * 0.30f, 9.0f, 16.0f);
    return {position.x + fx * lead, position.y, position.z + fz * lead};
}

#define OCEAN_WAVE_VECTOR Vector3
#define OCEAN_WAVE_MAKE(x, y, z) Vector3{x, y, z}
#define OCEAN_WAVE_FLOOR(value) std::floor(value)
#define OCEAN_WAVE_SQRT(value) std::sqrt(value)
#define OCEAN_WAVE_SIN(value) std::sin(value)
#define OCEAN_WAVE_COS(value) std::cos(value)
#define OCEAN_WAVE_INLINE inline
#include "../../resources/Shaders/Ocean/OceanWaveShape.hlsli"
#undef OCEAN_WAVE_VECTOR
#undef OCEAN_WAVE_MAKE
#undef OCEAN_WAVE_FLOOR
#undef OCEAN_WAVE_SQRT
#undef OCEAN_WAVE_SIN
#undef OCEAN_WAVE_COS
#undef OCEAN_WAVE_INLINE

inline Vector3 Displacement(float x, float z, float time, float amplitude, float frequency) {
    Vector3 result{};
#define OCEAN_WAVE(dx, dz, f, a, speed, steepness, phase) \
    { \
        const auto wave = EvaluateOceanWave(x, z, time, amplitude, frequency, dx, dz, f, a, speed, steepness, phase); \
        result.x += wave.displacement.x; \
        result.y += wave.displacement.y; \
        result.z += wave.displacement.z; \
    }
#include "../../resources/Shaders/Ocean/OceanWaveLayers.hlsli"
#undef OCEAN_WAVE
    return result;
}
inline float Height(float worldX, float worldZ, float time, float baseHeight, float amplitude, float frequency) {
    // Gerstner waves also move vertices horizontally. Invert that displacement before sampling Y.
    float x = worldX, z = worldZ;
    for (int i = 0; i < 6; ++i) {
        const auto wave = Displacement(x, z, time, amplitude, frequency);
        x = worldX - wave.x; z = worldZ - wave.z;
    }
    return baseHeight + Displacement(x, z, time, amplitude, frequency).y;
}
inline float Strength(float clearance, float speed) {
    float proximity = std::clamp((6.0f - clearance) / 5.5f, 0.0f, 1.0f);
    proximity = proximity * proximity * (3.0f - 2.0f * proximity);
    return proximity * std::clamp(speed / 30.0f, 0.0f, 1.0f);
}
}

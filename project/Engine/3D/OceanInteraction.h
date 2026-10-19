#pragma once
#include "Engine/Math/MathStruct.h"
#include <algorithm>
#include <cmath>

namespace OceanInteraction {
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

inline Vector3 Displacement(float x, float z, float time, float amplitude, float frequency) {
    const float noise = std::sin(x * 0.0041f + z * 0.0027f + time * 0.07f) * 0.5f +
        std::sin(x * -0.0019f + z * 0.0053f - time * 0.045f) * 0.5f;
    const float strength = 0.58f + (1.22f - 0.58f) * (noise * 0.5f + 0.5f);
    Vector3 result{};
    const auto add = [&](float dx, float dz, float f, float a, float speed, float steepness, float phaseOffset) {
        const float invLength = 1.0f / std::sqrt(dx * dx + dz * dz);
        dx *= invLength; dz *= invLength;
        const float phase = (dx * x + dz * z) * frequency * f + time * speed + phaseOffset;
        const float height = amplitude * a * strength;
        result.x += dx * steepness * height * std::cos(phase);
        result.y += height * std::sin(phase);
        result.z += dz * steepness * height * std::cos(phase);
    };
#define OCEAN_WAVE(dx, dz, f, a, speed, steepness, phase) add(dx, dz, f, a, speed, steepness, phase);
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

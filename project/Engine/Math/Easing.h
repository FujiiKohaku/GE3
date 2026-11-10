#pragma once
#include <algorithm>

namespace Easing {
inline float Clamp01(float value)
{
    return std::clamp(value, 0.0f, 1.0f);
}

inline float SmoothStep(float value)
{
    const float clamped = Clamp01(value);
    return clamped * clamped * (3.0f - 2.0f * clamped);
}

inline float EaseOutBack(float value)
{
    const float clamped = Clamp01(value);
    constexpr float kC1 = 1.702f;
    constexpr float kC3 = kC1 + 1.0f;
    const float shifted = clamped - 1.0f;
    return 1.0f + kC3 * shifted * shifted * shifted +
        kC1 * shifted * shifted;
}
inline float EaseInQuart(float time)
{
    time = std::clamp(time, 0.0f, 1.0f);
    return time * time * time * time;
}

inline float EaseInCubic(float time)
{
    time = std::clamp(time, 0.0f, 1.0f);
    return time * time * time;
}

}

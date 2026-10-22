#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

// 60FPSで調整した追従率と周期を、ゲーム内の経過秒へ変換する。
class GameTiming {
public:
    static float CalculateReferenceStep(float deltaTimeSeconds)
    {
        return (std::max)(deltaTimeSeconds, 0.0f) * kReferenceFramesPerSecond;
    }

    static float CalculateFollowBlend(float referenceBlend, float deltaTimeSeconds)
    {
        if (deltaTimeSeconds <= 0.0f) {
            return 0.0f;
        }
        const float blend = std::clamp(referenceBlend, 0.0f, 1.0f);
        return 1.0f - std::pow(1.0f - blend, deltaTimeSeconds * kReferenceFramesPerSecond);
    }

    // 周期の端数を維持する。上限を越えた発生分は捨て、次フレームへ大量に持ち越さない。
    static int32_t AdvanceInterval(float& elapsedSeconds, float intervalSeconds,
        float deltaTimeSeconds, int32_t maxEvents)
    {
        if (deltaTimeSeconds <= 0.0f || intervalSeconds <= 0.0f || maxEvents <= 0) {
            return 0;
        }
        elapsedSeconds += deltaTimeSeconds;
        const double eventCount = std::floor(
            (static_cast<double>(elapsedSeconds) + kTimeToleranceSeconds) / intervalSeconds);
        if (eventCount < 1.0) {
            return 0;
        }
        elapsedSeconds = static_cast<float>((std::max)(0.0,
            static_cast<double>(elapsedSeconds) - eventCount * intervalSeconds));
        return static_cast<int32_t>((std::min)(eventCount, static_cast<double>(maxEvents)));
    }

private:
    static constexpr float kReferenceFramesPerSecond = 60.0f;
    static constexpr double kTimeToleranceSeconds = 0.000001;
};

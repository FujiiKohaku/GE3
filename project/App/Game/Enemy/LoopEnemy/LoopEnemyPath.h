#pragma once

#include "Engine/math/MathStruct.h"
#include <algorithm>
#include <cmath>
#include <numbers>

struct LoopEnemyPathSample {
    Vector2 position {};
    Vector2 velocity {};
    bool hasFinished = false;
};

// 自機から見て右から進入し、上方へ一周して左へ抜ける軌道。
class LoopEnemyPath {
public:
    static constexpr float kEntryDurationSeconds = 2.0f;
    static constexpr float kLoopDurationSeconds = 4.0f;
    static constexpr float kExitDurationSeconds = 2.0f;
    static constexpr float kDurationSeconds = kEntryDurationSeconds + kLoopDurationSeconds + kExitDurationSeconds;
    static constexpr float kHorizontalRadius = 16.0f;
    static constexpr float kVerticalRadius = 14.0f;

    static LoopEnemyPathSample Sample(float elapsedSeconds, bool isEnteringFromLeft = false)
    {
        if (!std::isfinite(elapsedSeconds)) { elapsedSeconds = 0.0f; }
        const float timeSeconds = std::clamp(elapsedSeconds, 0.0f, kDurationSeconds);
        const float angularSpeedRadiansPerSecond = 2.0f * std::numbers::pi_v<float> / kLoopDurationSeconds;
        const float loopSpeed = kHorizontalRadius * angularSpeedRadiansPerSecond;
        LoopEnemyPathSample sample;
        if (timeSeconds <= kEntryDurationSeconds) {
            sample = SampleBezier({ 60.0f, 10.0f }, { 56.0f, 0.0f },
                { loopSpeed * kEntryDurationSeconds / 3.0f, 0.0f }, {},
                timeSeconds / kEntryDurationSeconds, kEntryDurationSeconds);
        } else if (timeSeconds <= kEntryDurationSeconds + kLoopDurationSeconds) {
            const float angleRadians = (timeSeconds - kEntryDurationSeconds) * angularSpeedRadiansPerSecond;
            sample.position = { -kHorizontalRadius * std::sin(angleRadians),
                kVerticalRadius * (1.0f - std::cos(angleRadians)) };
            sample.velocity = { -kHorizontalRadius * std::cos(angleRadians) * angularSpeedRadiansPerSecond,
                kVerticalRadius * std::sin(angleRadians) * angularSpeedRadiansPerSecond };
        } else {
            const float progress = (timeSeconds - kEntryDurationSeconds - kLoopDurationSeconds) / kExitDurationSeconds;
            sample = SampleBezier({}, { -loopSpeed * kExitDurationSeconds / 3.0f, 0.0f },
                { -44.0f, -3.0f }, { -64.0f, -3.0f }, progress, kExitDurationSeconds);
        }
        sample.hasFinished = elapsedSeconds >= kDurationSeconds;
        if (isEnteringFromLeft) {
            sample.position.x = -sample.position.x;
            sample.velocity.x = -sample.velocity.x;
        }
        return sample;
    }

private:
    static LoopEnemyPathSample SampleBezier(const Vector2& first, const Vector2& second,
        const Vector2& third, const Vector2& fourth, float progress, float durationSeconds)
    {
        const float remaining = 1.0f - progress;
        const float firstWeight = remaining * remaining * remaining;
        const float secondWeight = 3.0f * remaining * remaining * progress;
        const float thirdWeight = 3.0f * remaining * progress * progress;
        const float fourthWeight = progress * progress * progress;
        LoopEnemyPathSample sample;
        sample.position = {
            first.x * firstWeight + second.x * secondWeight + third.x * thirdWeight + fourth.x * fourthWeight,
            first.y * firstWeight + second.y * secondWeight + third.y * thirdWeight + fourth.y * fourthWeight };
        sample.velocity = {
            (3.0f * remaining * remaining * (second.x - first.x) + 6.0f * remaining * progress *
                (third.x - second.x) + 3.0f * progress * progress * (fourth.x - third.x)) / durationSeconds,
            (3.0f * remaining * remaining * (second.y - first.y) + 6.0f * remaining * progress *
                (third.y - second.y) + 3.0f * progress * progress * (fourth.y - third.y)) / durationSeconds };
        return sample;
    }
};

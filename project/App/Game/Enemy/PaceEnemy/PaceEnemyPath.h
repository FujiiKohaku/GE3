#pragma once
#include "Engine/Math/ValueValidation.h"

#include "Engine/LevelEditor/LevelData.h"
#include <cmath>
#include <numbers>

struct PaceEnemyPathSample {
    Vector3 position {};
    bool isPacing = false;
    bool hasFinished = false;
};

// レール基準の座標。横から進入し、指定時間並走して反対側へ離脱する。
class PaceEnemyPath {
public:
    static bool IsValidSettings(const LevelData::PaceEnemySpawnData& settings)
    {
        return ValueValidation::IsInRange(settings.activationDistance, 10.0f, 500.0f) &&
            ValueValidation::IsInRange(settings.entryDurationSeconds, 0.1f, 30.0f) &&
            ValueValidation::IsInRange(settings.holdDurationSeconds, 0.0f, 120.0f) &&
            ValueValidation::IsInRange(settings.exitDurationSeconds, 0.1f, 30.0f) &&
            ValueValidation::IsInRange(settings.forwardOffset, 10.0f, 150.0f) &&
            ValueValidation::IsInRange(settings.offsetX, -80.0f, 80.0f) &&
            ValueValidation::IsInRange(settings.offsetY, -80.0f, 80.0f) &&
            ValueValidation::IsInRange(settings.swayAmplitude, 0.0f, 40.0f) &&
            ValueValidation::IsInRange(settings.swayPeriodSeconds, 0.1f, 60.0f) &&
            ValueValidation::IsInRange(settings.sideTravelDistance, 40.0f, 500.0f);
    }

    static PaceEnemyPathSample Sample(float elapsedSeconds, const LevelData::PaceEnemySpawnData& settings)
    {
        PaceEnemyPathSample sample;
        sample.position = { settings.offsetX, settings.offsetY, settings.forwardOffset };
        float entryDirection = 1.0f;
        if (settings.isEnteringFromLeft) { entryDirection = -1.0f; }
        if (elapsedSeconds < settings.entryDurationSeconds) {
            float progress = elapsedSeconds / settings.entryDurationSeconds;
            if (progress < 0.0f) { progress = 0.0f; }
            const float smoothProgress = progress * progress * (3.0f - 2.0f * progress);
            sample.position.x += entryDirection * settings.sideTravelDistance * (1.0f - smoothProgress);
            return sample;
        }
        const float paceSeconds = elapsedSeconds - settings.entryDurationSeconds;
        if (paceSeconds < settings.holdDurationSeconds) {
            sample.position.x += settings.swayAmplitude *
                std::sin(2.0f * std::numbers::pi_v<float> * paceSeconds / settings.swayPeriodSeconds);
            sample.isPacing = true;
            return sample;
        }
        const float lastSway = settings.swayAmplitude *
            std::sin(2.0f * std::numbers::pi_v<float> * settings.holdDurationSeconds / settings.swayPeriodSeconds);
        float progress = (paceSeconds - settings.holdDurationSeconds) / settings.exitDurationSeconds;
        if (progress >= 1.0f) {
            progress = 1.0f;
            sample.hasFinished = true;
        }
        const float smoothProgress = progress * progress * (3.0f - 2.0f * progress);
        sample.position.x += lastSway * (1.0f - smoothProgress) - entryDirection * settings.sideTravelDistance * smoothProgress;
        return sample;
    }

};

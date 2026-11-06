#pragma once

#include "Engine/LevelEditor/LevelData.h"
#include <algorithm>
#include <cmath>

struct StrafeEnemyPathSample {
    Vector3 position {};
    bool hasFinished = false;
};

class StrafeEnemyPath {
public:
    static bool IsValidSettings(const LevelData::StrafeEnemySpawnData& settings)
    {
        return IsInRange(settings.activationDistance, 10.0f, 500.0f) &&
            IsInRange(settings.durationSeconds, 1.0f, 20.0f) &&
            IsInRange(settings.forwardOffset, 10.0f, 150.0f) &&
            IsInRange(settings.offsetX, -80.0f, 80.0f) && IsInRange(settings.offsetY, -80.0f, 80.0f) &&
            IsInRange(settings.sideDistance, 40.0f, 200.0f) && settings.shotCount >= 1 && settings.shotCount <= 8 &&
            IsInRange(settings.fireStartProgress, 0.0f, 0.9f) &&
            IsInRange(settings.fireEndProgress, 0.0f, 0.9f) && settings.fireEndProgress >= settings.fireStartProgress &&
            (settings.shotCount == 1 || settings.fireEndProgress > settings.fireStartProgress);
    }

    static StrafeEnemyPathSample Sample(float elapsedSeconds, const LevelData::StrafeEnemySpawnData& settings)
    {
        StrafeEnemyPathSample sample;
        const float progress = std::clamp(elapsedSeconds / settings.durationSeconds, 0.0f, 1.0f);
        float entryDirection = 1.0f;
        if (settings.isEnteringFromLeft) { entryDirection = -1.0f; }
        sample.position = { settings.offsetX + entryDirection * settings.sideDistance * (1.0f - 2.0f * progress),
            settings.offsetY, settings.forwardOffset };
        sample.hasFinished = elapsedSeconds >= settings.durationSeconds;
        return sample;
    }

    static int GetScheduledShotCount(float elapsedSeconds, const LevelData::StrafeEnemySpawnData& settings)
    {
        int count = 0;
        for (int shotIndex = 0; shotIndex < settings.shotCount; ++shotIndex) {
            float progress = settings.fireStartProgress;
            if (settings.shotCount > 1) {
                progress += (settings.fireEndProgress - settings.fireStartProgress) *
                    static_cast<float>(shotIndex) / static_cast<float>(settings.shotCount - 1);
            }
            if (elapsedSeconds + 0.00001f >= progress * settings.durationSeconds) { ++count; }
        }
        return count;
    }

private:
    static bool IsInRange(float value, float minimum, float maximum)
    {
        return std::isfinite(value) && value >= minimum && value <= maximum;
    }
};

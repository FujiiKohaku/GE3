#pragma once
#include <cmath>
#include <cstdint>

struct NormalEnemySettings {
    float hp = 4.0f;
    int32_t fireIntervalFrames = 120;
    float bulletSpeedPerFrame = 0.48f;
    bool isRadialShotEnabled = true;
};

inline bool IsValidNormalEnemySettings(const NormalEnemySettings& settings)
{
    return std::isfinite(settings.hp) && settings.hp > 0.0f &&
        settings.fireIntervalFrames > 0 && std::isfinite(settings.bulletSpeedPerFrame) &&
        settings.bulletSpeedPerFrame > 0.0f;
}

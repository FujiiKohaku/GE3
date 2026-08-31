#include "Engine/Time/TimeManager.h"

#include <algorithm>
#include <cmath>

TimeManager* TimeManager::GetInstance()
{
    static TimeManager instance;
    return &instance;
}

void TimeManager::Initialize()
{
    previousTime_ = Clock::now();
    deltaTime_ = 0.0f;
    unscaledDeltaTime_ = 0.0f;
    timeScale_ = 1.0f;
    totalTime_ = 0.0;
    unscaledTotalTime_ = 0.0;
    frameCount_ = 0;
    isInitialized_ = true;
}

void TimeManager::Update()
{
    const Clock::time_point currentTime = Clock::now();
    if (!isInitialized_) {
        Initialize();
        return;
    }

    const float measuredDeltaTime =
        std::chrono::duration<float>(currentTime - previousTime_).count();
    previousTime_ = currentTime;

    unscaledDeltaTime_ = std::clamp(measuredDeltaTime, 0.0f, maxDeltaTime_);
    deltaTime_ = unscaledDeltaTime_ * timeScale_;
    unscaledTotalTime_ += static_cast<double>(unscaledDeltaTime_);
    totalTime_ += static_cast<double>(deltaTime_);
    ++frameCount_;
}

void TimeManager::SetTimeScale(float timeScale)
{
    if (!std::isfinite(timeScale)) {
        return;
    }
    timeScale_ = std::clamp(timeScale, 0.0f, 100.0f);
}

void TimeManager::SetMaxDeltaTime(float maxDeltaTime)
{
    if (!std::isfinite(maxDeltaTime) || maxDeltaTime <= 0.0f) {
        return;
    }
    maxDeltaTime_ = maxDeltaTime;
}

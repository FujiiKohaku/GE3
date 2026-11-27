#pragma once
#include <algorithm>

class BossPartHitScale final {
public:
    void Start() { remainingSeconds_ = kDurationSeconds; }

    float Update(float deltaTimeSeconds)
    {
        if (remainingSeconds_ <= 0.0f) { return 1.0f; }

        const float weight = remainingSeconds_ / kDurationSeconds;
        remainingSeconds_ = (std::max)(0.0f, remainingSeconds_ - (std::max)(0.0f, deltaTimeSeconds));
        return 1.0f + kScaleIncrease * weight * weight;
    }

private:
    static constexpr float kDurationSeconds = 0.15f;
    static constexpr float kScaleIncrease = 0.20f;
    float remainingSeconds_ = 0.0f;
};

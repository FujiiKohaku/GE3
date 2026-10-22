#pragma once

#include "../State/IceJellyfishState.h"

class IceJellyfishDeadState final : public IceJellyfishState {
public:
    void Reset();
    void Enter(IceJellyfish& boss) override;
    void Update(IceJellyfish& boss) override;
    bool IsDeathSequenceFinished() const { return elapsedSeconds_ >= kDurationSeconds; }

private:
    friend bool RunIceJellyfishStateTests();
    static constexpr float kDurationSeconds = 2.0f;
    static constexpr float kFallSpeedUnitsPerSecond = 9.0f;
    static constexpr float kForwardSpeedUnitsPerSecond = 5.0f;
    float elapsedSeconds_ = 0.0f;
};

#pragma once

#include "../State/FearWormState.h"
#include "Engine/Math/MathStruct.h"

// 静止、崩壊予兆、破片の飛散と余韻を順に進める。
class FearWormDeadState final : public FearWormState {
public:
    void Enter(FearWormEnemy& boss) override;
    void Update(FearWormEnemy& boss) override;
    bool IsDeathSequenceFinished() const { return isDeathSequenceFinished_; }
    float GetCameraShakeStrength() const;

private:
    friend bool RunFearWormStateTests();

    static constexpr float kFreezeDurationSeconds = 0.18f;
    static constexpr float kBurstStartSeconds = 0.95f;
    static constexpr float kDeathDurationSeconds = 3.65f;
    static constexpr float kImpactShakeDurationSeconds = 0.12f;
    static constexpr float kShakeDecayDurationSeconds = 1.1f;
    static constexpr float kImpactShakeStrength = 0.035f;
    static constexpr float kAftershockShakeStrength = 0.024f;

    void Burst(FearWormEnemy& boss);

    bool isDeathSequenceFinished_ = false;
    float elapsedSeconds_ = 0.0f;
    bool hasPlayedPrelude_ = false;
    bool hasBurst_ = false;
    Vector3 origin_ {};
    Vector3 rotationRadians_ {};
};

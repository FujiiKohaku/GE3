#pragma once

#include "../State/FearWormState.h"
#include "Engine/Math/MathStruct.h"

// 死亡演出の進行と、その間だけ使用する落下・回転の状態を所有する。
class FearWormDeadState final : public FearWormState {
public:
    void Enter(FearWormEnemy& boss) override;
    void Update(FearWormEnemy& boss) override;
    bool IsDeathSequenceFinished() const { return isDeathSequenceFinished_; }

private:
    friend bool RunFearWormStateTests();

    static constexpr float kGravityUnitsPerSecondSquared = 26.0f;
    static constexpr float kDeathFallbackDurationSeconds = 1.6f;
    static constexpr float kDeathFinishHeight = -35.0f;
    static constexpr Vector3 kInitialVelocityUnitsPerSecond = { 0.0f, 11.5f, -9.0f };
    static constexpr Vector3 kAngularVelocityRadiansPerSecond = { 1.8f, 2.5f, 0.8f };

    bool isDeathSequenceFinished_ = false;
    float elapsedSeconds_ = 0.0f;
    Vector3 velocityUnitsPerSecond_ {};
    Vector3 rotationRadians_ {};
};

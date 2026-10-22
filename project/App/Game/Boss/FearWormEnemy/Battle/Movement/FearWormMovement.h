#pragma once

#include "Engine/Math/MathStruct.h"

// 登場と戦闘で共有する、プレイヤーを基準にした軌道の入力。
struct FearWormMovementContext {
    Vector3 playerPosition;
    float orbitAngle;
    float forwardOffset;
};

// 複数の行動で共有する軌道。攻撃固有の時間や段階は保持しない。
class FearWormMovement {
public:
    static constexpr float kOrbitAngularSpeed = 1.15f;
    static Vector3 CalculateOrbitTarget(const FearWormMovementContext& context);
    static Vector3 CalculateDriftTarget(const FearWormMovementContext& context);

private:
    static constexpr float kOrbitRadiusX = 16.0f;
    static constexpr float kOrbitRadiusY = 7.0f;
    static constexpr float kOrbitHeight = 7.0f;
    static constexpr float kOrbitLateralSwayFrequency = 2.0f;
    static constexpr float kOrbitLateralSwayAmplitude = 2.0f;
    static constexpr float kOrbitDepthSwayFrequency = 0.80f;
    static constexpr float kOrbitDepthSwayAmplitude = 3.0f;
    static constexpr float kDriftHorizontalFrequency = 0.65f;
    static constexpr float kDriftHorizontalAmplitude = 8.0f;
    static constexpr float kDriftHeight = 8.0f;
    static constexpr float kDriftVerticalFrequency = 0.85f;
    static constexpr float kDriftVerticalAmplitude = 4.5f;
    static constexpr float kDriftDepthFrequency = 0.50f;
    static constexpr float kDriftDepthAmplitude = 2.0f;
};

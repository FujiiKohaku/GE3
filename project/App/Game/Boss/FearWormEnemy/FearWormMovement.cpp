#include "FearWormMovement.h"
#include <algorithm>
#include <cmath>

namespace {
constexpr float kOrbitRadiusX = 16.0f;
constexpr float kOrbitRadiusY = 7.0f;
constexpr float kOrbitHeight = 7.0f;
constexpr float kOrbitLateralSwayFrequency = 2.0f;
constexpr float kOrbitLateralSwayAmplitude = 2.0f;
constexpr float kOrbitDepthSwayFrequency = 0.80f;
constexpr float kOrbitDepthSwayAmplitude = 3.0f;
constexpr float kCoilAngleRate = 2.35f;
constexpr float kCoilBaseRadius = 8.0f;
constexpr float kCoilRadiusFrequency = 3.2f;
constexpr float kCoilRadiusAmplitude = 2.0f;
constexpr float kCoilHeight = 8.0f;
constexpr float kCoilVerticalScale = 0.75f;
constexpr float kCoilDepthFrequency = 0.50f;
constexpr float kCoilDepthAmplitude = 2.5f;
constexpr float kWeaveAngleRate = 1.35f;
constexpr float kWeaveWidth = 13.0f;
constexpr float kWeaveHeight = 7.0f;
constexpr float kWeaveVerticalFrequency = 2.0f;
constexpr float kWeaveVerticalAmplitude = 9.0f;
constexpr float kWeaveDepthAmplitude = 2.5f;
constexpr float kDriftHorizontalFrequency = 0.65f;
constexpr float kDriftHorizontalAmplitude = 8.0f;
constexpr float kDriftHeight = 8.0f;
constexpr float kDriftVerticalFrequency = 0.85f;
constexpr float kDriftVerticalAmplitude = 4.5f;
constexpr float kDriftDepthFrequency = 0.50f;
constexpr float kDriftDepthAmplitude = 2.0f;

Vector3 CalculateOrbitTarget(const FearWormMovementContext& context)
{
    Vector3 result = context.playerPosition;

    // 基本の楕円軌道を計算する。
    float orbitX = std::cos(context.orbitAngle) * kOrbitRadiusX;

    float orbitY = std::sin(context.orbitAngle) * kOrbitRadiusY;

    // 単調な周回にならないよう、横揺れと奥行きの揺れを加える。
    float lateralSway = std::sin(context.orbitAngle * kOrbitLateralSwayFrequency) * kOrbitLateralSwayAmplitude;

    float depthSway = std::sin(context.orbitAngle * kOrbitDepthSwayFrequency) * kOrbitDepthSwayAmplitude;

    result.x += orbitX + lateralSway;
    result.y += kOrbitHeight + orbitY;
    result.z += context.forwardOffset + depthSway;

    return result;
}

Vector3 CalculateCoilTarget(const FearWormMovementContext& context)
{
    Vector3 result = context.playerPosition;

    // 通常の軌道角を速めて、巻き付くような回転角を作る。
    float coilAngle = context.orbitAngle * kCoilAngleRate;

    // 時間経過で半径を伸縮させる。
    float radius = kCoilBaseRadius + std::sin(context.patternTime * kCoilRadiusFrequency) * kCoilRadiusAmplitude;

    result.x += std::cos(coilAngle) * radius;
    result.y += kCoilHeight + std::sin(coilAngle) * radius * kCoilVerticalScale;
    result.z += context.forwardOffset + std::sin(coilAngle * kCoilDepthFrequency) * kCoilDepthAmplitude;

    return result;
}

Vector3 CalculateWeaveTarget(const FearWormMovementContext& context)
{
    Vector3 result = context.playerPosition;

    // 横方向と縦方向で異なる周期を使い、波打つ軌道を作る。
    float waveAngle = context.orbitAngle * kWeaveAngleRate;

    result.x += std::sin(waveAngle) * kWeaveWidth;
    result.y += kWeaveHeight + std::sin(waveAngle * kWeaveVerticalFrequency) * kWeaveVerticalAmplitude;
    result.z += context.forwardOffset + std::cos(waveAngle) * kWeaveDepthAmplitude;

    return result;
}

Vector3 CalculateDriftTarget(const FearWormMovementContext& context)
{
    Vector3 result = context.playerPosition;

    // 各軸を異なる周期で揺らし、緩やかに漂う軌道を作る。
    result.x += std::sin(context.orbitAngle * kDriftHorizontalFrequency) * kDriftHorizontalAmplitude;
    result.y += kDriftHeight + std::sin(context.orbitAngle * kDriftVerticalFrequency) * kDriftVerticalAmplitude;
    result.z += context.forwardOffset + std::cos(context.orbitAngle * kDriftDepthFrequency) * kDriftDepthAmplitude;

    return result;
}

Vector3 CalculateSpiralTarget(const FearWormMovementContext& context)
{
    Vector3 result = context.playerPosition;
    float baseDistZ = context.forwardOffset + 50.0f;

    // 最初の2秒間は画面中央に静止して威嚇
    if (context.madModeTime < 2.0f) {
        result.x += 0.0f;
        result.y += 7.0f;
        result.z += baseDistZ;
    } else {
        // 後半の5秒間で、中心から外側へ渦を描くように広がる (半径 0.0f 〜 25.0f)
        float t = (context.madModeTime - 2.0f) / 5.0f;
        float spiralRadius = t * 25.0f;

        result.x += std::cos(context.spiralAngle) * spiralRadius;
        result.y += 7.0f + std::sin(context.spiralAngle) * spiralRadius;
        result.z += baseDistZ;
    }

    // 地面にめり込まないように下限（Y = 1.5f）をかける
    if (result.y < 1.5f) {
        result.y = 1.5f;
    }

    return result;
}

Vector3 CalculateLineTarget(const FearWormMovementContext& context)
{
    Vector3 result = context.playerPosition;
    result.x += 20.0f;
    result.y += 3.0f;
    result.z += context.forwardOffset + 75.0f;
    return result;
}

} // namespace

Vector3 CalculateFearWormTarget(FearWormMovementType type, const FearWormMovementContext& context)
{
    switch (type) {
    case FearWormMovementType::Coil: return CalculateCoilTarget(context);
    case FearWormMovementType::Weave: return CalculateWeaveTarget(context);
    case FearWormMovementType::Drift: return CalculateDriftTarget(context);
    case FearWormMovementType::Line: return CalculateLineTarget(context);
    case FearWormMovementType::Spiral: return CalculateSpiralTarget(context);
    default: return CalculateOrbitTarget(context);
    }
}

FearWormMovementType NextFearWormMovement(FearWormMovementType type)
{
    switch (type) {
    case FearWormMovementType::Orbit: return FearWormMovementType::Coil;
    case FearWormMovementType::Coil: return FearWormMovementType::Weave;
    case FearWormMovementType::Weave: return FearWormMovementType::Drift;
    case FearWormMovementType::Drift: return FearWormMovementType::Orbit;
    default: return type;
    }
}

#include "BaseFearWormMovement.h"
#include <algorithm>
#include <array>
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
}

class OrbitFearWormMovement final : public BaseFearWormMovement {
public:
    OrbitFearWormMovement() : BaseFearWormMovement(FearWormMovementType::Orbit, FearWormMovementType::Coil) {}
    Vector3 CalculateTarget(const FearWormMovementContext& context) const override
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
};

class CoilFearWormMovement final : public BaseFearWormMovement {
public:
    CoilFearWormMovement() : BaseFearWormMovement(FearWormMovementType::Coil, FearWormMovementType::Weave) {}
    Vector3 CalculateTarget(const FearWormMovementContext& context) const override
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
};

class WeaveFearWormMovement final : public BaseFearWormMovement {
public:
    WeaveFearWormMovement() : BaseFearWormMovement(FearWormMovementType::Weave, FearWormMovementType::Drift) {}
    Vector3 CalculateTarget(const FearWormMovementContext& context) const override
    {
        Vector3 result = context.playerPosition;

        // 横方向と縦方向で異なる周期を使い、波打つ軌道を作る。
        float waveAngle = context.orbitAngle * kWeaveAngleRate;

        result.x += std::sin(waveAngle) * kWeaveWidth;
        result.y += kWeaveHeight + std::sin(waveAngle * kWeaveVerticalFrequency) * kWeaveVerticalAmplitude;
        result.z += context.forwardOffset + std::cos(waveAngle) * kWeaveDepthAmplitude;

        return result;
    }
};

class DriftFearWormMovement final : public BaseFearWormMovement {
public:
    DriftFearWormMovement() : BaseFearWormMovement(FearWormMovementType::Drift, FearWormMovementType::Orbit) {}
    Vector3 CalculateTarget(const FearWormMovementContext& context) const override
    {
        Vector3 result = context.playerPosition;

        // 各軸を異なる周期で揺らし、緩やかに漂う軌道を作る。
        result.x += std::sin(context.orbitAngle * kDriftHorizontalFrequency) * kDriftHorizontalAmplitude;
        result.y += kDriftHeight + std::sin(context.orbitAngle * kDriftVerticalFrequency) * kDriftVerticalAmplitude;
        result.z += context.forwardOffset + std::cos(context.orbitAngle * kDriftDepthFrequency) * kDriftDepthAmplitude;

        return result;
    }
};

class SpiralFearWormMovement final : public BaseFearWormMovement {
public:
    SpiralFearWormMovement() : BaseFearWormMovement(FearWormMovementType::Spiral, FearWormMovementType::Spiral, false) {}
    void Enter(FearWormMovementState) const override {}
    Vector3 CalculateTarget(const FearWormMovementContext& context) const override
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
};

class LineFearWormMovement final : public BaseFearWormMovement {
public:
    LineFearWormMovement() : BaseFearWormMovement(FearWormMovementType::Line, FearWormMovementType::Line, false) {}
    void Enter(FearWormMovementState state) const override { state.lineTransitionTime = 0.0f; }
    float UpdateLineBlend(FearWormMovementState state, float dt) const override {
        state.lineTransitionTime += dt;
        return (std::min)(state.lineTransitionTime, 1.0f);
    }
    Vector3 CalculateTarget(const FearWormMovementContext& context) const override {
        Vector3 result = context.playerPosition;
        result.x += 20.0f;
        result.y += 3.0f;
        result.z += context.forwardOffset + 75.0f;
        return result;
    }
};

const BaseFearWormMovement& GetFearWormMovement(FearWormMovementType type)
{
    static const OrbitFearWormMovement orbit;
    static const CoilFearWormMovement coil;
    static const WeaveFearWormMovement weave;
    static const DriftFearWormMovement drift;
    static const LineFearWormMovement line;
    static const SpiralFearWormMovement spiral;
    static const std::array<const BaseFearWormMovement*, 6> movements { &orbit, &coil, &weave, &drift, &line, &spiral };
    const auto index = static_cast<size_t>(type);
    return *movements[index < movements.size() ? index : 0];
}
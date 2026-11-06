#pragma once

#include "Engine/math/EngineStruct.h"
#include <algorithm>
#include <cmath>
#include <numbers>

struct TurretEnemyPose {
    Matrix4x4 baseWorld {};
    Matrix4x4 yawWorld {};
    Matrix4x4 barrelWorld {};
    Vector3 muzzlePosition {};
    Vector3 forward {};
};

class TurretEnemyAim {
public:
    static constexpr float kDisplayScale = 3.5f;
    static constexpr float kTurnSpeedRadiansPerSecond = std::numbers::pi_v<float> * 0.5f;
    static constexpr float kMinimumPitchRadians = -std::numbers::pi_v<float> / 3.0f;
    static constexpr float kMaximumPitchRadians = std::numbers::pi_v<float> / 18.0f;

    static float ApproachAngle(float current, float target, float deltaTimeSeconds)
    {
        const float difference = std::remainder(target - current, 2.0f * std::numbers::pi_v<float>);
        const float maximumStep = kTurnSpeedRadiansPerSecond * (std::max)(0.0f, deltaTimeSeconds);
        return std::remainder(current + std::clamp(difference, -maximumStep, maximumStep),
            2.0f * std::numbers::pi_v<float>);
    }

    static bool CalculateTargetAngles(const Matrix4x4& baseWorld, const Vector3& targetPosition,
        float& yawRadians, float& pitchRadians)
    {
        const Vector3 localTarget = MatrixMath::Transform(targetPosition, MatrixMath::Inverse(baseWorld)) -
            Vector3 { 0.0f, 1.12f, 0.0f };
        if (!std::isfinite(localTarget.x) || !std::isfinite(localTarget.y) || !std::isfinite(localTarget.z) ||
            Vector3Length(localTarget) < 0.0001f) { return false; }
        // このエンジンのY回転は、正面+Zを負のXへ回す符号規約。
        yawRadians = -std::atan2(localTarget.x, localTarget.z);
        const float horizontalDistance = std::sqrt(localTarget.x * localTarget.x + localTarget.z * localTarget.z);
        pitchRadians = std::clamp(-std::atan2(localTarget.y, horizontalDistance), kMinimumPitchRadians, kMaximumPitchRadians);
        return true;
    }

    static TurretEnemyPose CalculatePose(const EulerTransform& transform, float yawRadians, float pitchRadians,
        float recoilDistance = 0.0f)
    {
        TurretEnemyPose pose;
        pose.baseWorld = MatrixMath::MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
        const Matrix4x4 yawLocal = MatrixMath::MakeAffineMatrix(Vector3 { 1.0f, 1.0f, 1.0f },
            Vector3 { 0.0f, yawRadians, 0.0f }, Vector3 { 0.0f, 0.62f, 0.0f });
        pose.yawWorld = MatrixMath::Multiply(yawLocal, pose.baseWorld);
        const Matrix4x4 barrelLocal = MatrixMath::MakeAffineMatrix(Vector3 { 1.0f, 1.0f, 1.0f },
            Vector3 { pitchRadians, 0.0f, 0.0f }, Vector3 { 0.0f, 0.50f, 0.0f });
        pose.barrelWorld = MatrixMath::Multiply(barrelLocal, pose.yawWorld);
        pose.barrelWorld = MatrixMath::Multiply(MatrixMath::MakeTranslateMatrix({ 0.0f, 0.0f, -recoilDistance }), pose.barrelWorld);
        pose.muzzlePosition = MatrixMath::Transform(Vector3 { 0.0f, 0.03f, 2.28f }, pose.barrelWorld);
        pose.forward = Normalize(MatrixMath::Transform(Vector3 { 0.0f, 0.0f, 1.0f }, pose.barrelWorld) -
            MatrixMath::Transform(Vector3 {}, pose.barrelWorld));
        return pose;
    }
};

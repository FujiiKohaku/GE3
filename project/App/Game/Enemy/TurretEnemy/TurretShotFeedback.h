#pragma once

#include "Engine/math/MatrixMath.h"
#include <algorithm>
#include <cmath>

class TurretShotFeedback {
public:
    static constexpr float kFlashDurationSeconds = 0.07f;
    static constexpr float kRecoilKickSeconds = 0.035f;
    static constexpr float kRecoilDurationSeconds = 0.24f;
    static constexpr float kRecoilDistance = 0.20f;

    static float CalculateRecoil(float elapsedSeconds)
    {
        if (elapsedSeconds <= 0.0f || elapsedSeconds >= kRecoilDurationSeconds) { return 0.0f; }
        if (elapsedSeconds < kRecoilKickSeconds) {
            const float progress = elapsedSeconds / kRecoilKickSeconds;
            return kRecoilDistance * (1.0f - (1.0f - progress) * (1.0f - progress));
        }
        const float progress = (elapsedSeconds - kRecoilKickSeconds) / (kRecoilDurationSeconds - kRecoilKickSeconds);
        return kRecoilDistance * (1.0f - progress) * (1.0f - progress);
    }

    static Matrix4x4 CalculateTrailWorld(const Vector3& headPosition, const Vector3& forward,
        float length, float width)
    {
        Vector3 referenceAxis { 0.0f, 1.0f, 0.0f };
        if (std::abs(Dot(forward, referenceAxis)) > 0.99f) { referenceAxis = { 1.0f, 0.0f, 0.0f }; }
        const Vector3 right = Normalize(Cross(referenceAxis, forward));
        const Vector3 up = Normalize(Cross(forward, right));
        const Vector3 origin = headPosition - forward * length;
        Matrix4x4 matrix = MatrixMath::MakeIdentity4x4();
        const Vector3 axes[3] { right * width, up * width, forward * length };
        for (int index = 0; index < 3; ++index) {
            matrix.m[index][0] = axes[index].x;
            matrix.m[index][1] = axes[index].y;
            matrix.m[index][2] = axes[index].z;
        }
        matrix.m[3][0] = origin.x;
        matrix.m[3][1] = origin.y;
        matrix.m[3][2] = origin.z;
        return matrix;
    }
};

class TurretShotCycle {
public:
    static constexpr float kInitialDelaySeconds = 0.35f;

    void Reset()
    {
        elapsedSeconds_ = 0.0f;
        hasFired_ = false;
    }

    bool Advance(bool isInRange, bool isAligned, float deltaTimeSeconds, float intervalSeconds)
    {
        if (!isInRange) { Reset(); return false; }
        if (!std::isfinite(deltaTimeSeconds) || deltaTimeSeconds <= 0.0f ||
            !std::isfinite(intervalSeconds) || intervalSeconds <= 0.0f) { return false; }
        float delaySeconds = kInitialDelaySeconds;
        if (hasFired_) { delaySeconds = intervalSeconds; }
        if (!hasFired_ && !isAligned) { elapsedSeconds_ = 0.0f; return false; }
        elapsedSeconds_ = (std::min)(delaySeconds, elapsedSeconds_ + deltaTimeSeconds);
        if (!isAligned || elapsedSeconds_ + 0.000001f < delaySeconds) { return false; }
        elapsedSeconds_ = 0.0f;
        hasFired_ = true;
        return true;
    }

private:
    float elapsedSeconds_ = 0.0f;
    bool hasFired_ = false;
};

#pragma once
#include "Engine/math/MathStruct.h"

class Camera;
struct Ray;

// レティクルの画面位置からRayを作り、補間済みの射撃先を求める。
class PlayerShotPointCalculator {
public:
    void Initialize();
    Vector3 CalculateForwardPoint(const Vector3& shipPosition, const Vector3& shipDirection) const;
    void Update(const Camera& activeCamera, const Vector2& aimScreenPosition, float deltaTimeSeconds);
    void UpdateFromWorldPoint(const Camera& activeCamera, const Vector2& aimScreenPosition,
        const Vector3& forwardPoint, float deltaTimeSeconds);
    bool IsAimDistanceSmoothingEnabled() const { return isAimDistanceSmoothingEnabled_; }
    void SetAimDistanceSmoothingEnabled(bool isEnabled) { isAimDistanceSmoothingEnabled_ = isEnabled; }
    float GetAimDistance() const { return smoothedAimDistance_; }
    Vector3 GetAimPoint(const Camera& activeCamera, const Vector2& aimScreenPosition) const;
    Vector3 GetAimRayOrigin(const Camera& activeCamera, const Vector2& aimScreenPosition) const;

private:
    void CreateAimRay(Ray& aimRay, const Camera& activeCamera, const Vector2& aimScreenPosition) const;
    void UpdateFromRay(const Ray& aimRay, const Camera& activeCamera, const Vector2& aimScreenPosition,
        float deltaTimeSeconds, float fallbackDistance);
    bool IsAimCurrent(const Camera& activeCamera, const Vector2& aimScreenPosition) const;
    static constexpr float kAimConvergenceDistance = 220.0f;
    static constexpr float kAimDistanceFollowSpeed = 12.0f;
    bool isAimDistanceSmoothingEnabled_ = true;
    float smoothedAimDistance_ = kAimConvergenceDistance;
    Vector3 aimPoint_ {};
    Vector3 aimRayOrigin_ {};
    const Camera* aimCamera_ = nullptr;
    Vector2 cachedAimScreenPosition_ {};
    float cachedScreenWidthPixels_ = 0.0f;
    float cachedScreenHeightPixels_ = 0.0f;
    Matrix4x4 cachedViewMatrix_ {};
    Matrix4x4 cachedProjectionMatrix_ {};
};

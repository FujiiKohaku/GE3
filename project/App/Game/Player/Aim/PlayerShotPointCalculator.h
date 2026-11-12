#pragma once
#include "Engine/math/MathStruct.h"

class Camera;
struct Ray;

// レティクルの画面位置からRayを作り、射撃先を求める。
class PlayerShotPointCalculator {
public:
    static constexpr float GetAimConvergenceDistance() { return kAimConvergenceDistance; }

    void Initialize();
    void Update(const Camera& activeCamera, const Vector2& aimScreenPosition, float deltaTimeSeconds);
    float GetAimDistance() const { return aimDistance_; }
    Vector3 GetAimPoint() const;
    Vector3 GetAimRayOrigin() const;

private:
    void CreateAimRay(Ray& aimRay, const Camera& activeCamera, const Vector2& aimScreenPosition) const;
    void UpdateFromRay(const Ray& aimRay, float fallbackDistance);
    static constexpr float kAimConvergenceDistance = 220.0f;
    float aimDistance_ = kAimConvergenceDistance;
    Vector3 aimPoint_ {};
    Vector3 aimRayOrigin_ {};
};

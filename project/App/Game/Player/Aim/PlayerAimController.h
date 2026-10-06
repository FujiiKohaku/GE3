#pragma once
#include "Engine/math/MathStruct.h"

class Camera;
struct Ray;

class PlayerAimController {
public:
    void Initialize();
    void UpdateMouseAim();
    void ClampAimScreenPosition();
    void CreateAimRay(Ray& aimRay, const Camera& activeCamera) const;
    Vector3 CreateConvergencePoint(const Ray& aimRay) const;
    void UpdateSmoothedAimDistance(const Camera& activeCamera, float deltaTimeSeconds);
    Vector3 ResolveAimPoint(const Ray& aimRay) const;
    const Vector2& GetAimScreenPosition() const { return aimScreenPosition_; }

private:
    static constexpr float kAimConvergenceDistance = 220.0f;
    static constexpr float kAimDistanceFollowSpeed = 12.0f;
    Vector2 aimScreenPosition_ = { 0.0f, 0.0f };
    float smoothedAimDistance_ = kAimConvergenceDistance;
};

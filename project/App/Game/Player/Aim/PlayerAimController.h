#pragma once
#include "Engine/math/MathStruct.h"

// PlayerAimController
class Camera;
struct Ray;

// 照準の画面位置、入力による移動、射撃先の距離を管理する。
class PlayerAimController {
public:
    static constexpr Vector2 kReticleSizeUi = { 128.0f, 128.0f };
    void InitializeAim();
    void UpdateMouseAim();
    void UpdateGamepadAim(float stickX, float stickY, bool isPitchInverted, float deltaTimeSeconds, float sensitivity = 1.0f);
    void ClampAimScreenPosition();
    void ClampGamepadAimToPlayer(const Vector2& playerScreenPosition);
    Vector2 GetGamepadSteeringScreenPosition() const;
    const Vector2& GetAimScreenPosition() const { return aimScreenPosition_; }
    void Update(const Camera& activeCamera, float deltaTimeSeconds);
    bool IsAimDistanceSmoothingEnabled() const { return isAimDistanceSmoothingEnabled_; }
    void SetAimDistanceSmoothingEnabled(bool isEnabled) { isAimDistanceSmoothingEnabled_ = isEnabled; }
    float GetAimDistance() const { return smoothedAimDistance_; }
    Vector3 GetAimPoint(const Camera& activeCamera) const;
    Vector3 GetAimRayOrigin(const Camera& activeCamera) const;

private:
    void CreateAimRay(Ray& aimRay, const Camera& activeCamera) const;
    bool IsAimCurrent(const Camera& activeCamera) const;
    static constexpr float kGamepadMaxAimDistanceRatio = 0.40f;
    const float kAimConvergenceDistance = 220.0f;
    const float kAimDistanceFollowSpeed = 12.0f;
    Vector2 aimScreenPosition_ = { 0.0f, 0.0f };
    Vector2 gamepadAimOffset_ = {};
    Vector2 gamepadAimAnchor_ = {};
    bool hasGamepadAimAnchor_ = false;
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

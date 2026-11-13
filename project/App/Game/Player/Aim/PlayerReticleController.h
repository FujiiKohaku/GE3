#pragma once
#include "Engine/math/MathStruct.h"

// PlayerReticleController
class Camera;
class Input;

// レティクルの画面位置、入力による移動、表示範囲の制限を管理する。
class PlayerReticleController {
public:
    static constexpr Vector2 kReticleSizeUi = { 128.0f, 128.0f };
    static constexpr float kDirectAimYawRadians = 0.45f;
    static constexpr float kDirectAimPitchRadians = 0.33f;
    // EX-ZODIAC風操作の入力を更新し、機体へ渡す操舵入力を返す。
    Vector2 UpdateDirectGamepadControl(const Input& input, float deltaTimeSeconds);
    void ResetDirectGamepadAim() { directGamepadAimInput_ = {}; }
    const Vector2& GetDirectGamepadAimInput() const { return directGamepadAimInput_; }
    Vector3 GetDirectGamepadAimDirection(bool isAllRangeMode, const Vector3& forward,
        const Vector3& right, const Vector3& up) const;
    void UpdateDirectGamepadAim(const Camera& activeCamera, const Vector3& playerPosition,
        bool isAllRangeMode, const Vector3& forward, const Vector3& right, const Vector3& up,
        float convergenceDistance);
    Vector3 GetNearAimPoint(const Vector3& muzzlePosition, const Vector3& aimPoint) const;
    void InitializeAim();
    void UpdateMouseAim();
    void ClampAimScreenPosition();
    const Vector2& GetAimScreenPosition() const { return aimScreenPosition_; }
    void UpdateFromWorldPoint(const Camera& activeCamera, const Vector3& worldPoint);

private:
    static constexpr Vector2 kHalfReticleSizeUi = {
        kReticleSizeUi.x * 0.5f,
        kReticleSizeUi.y * 0.5f
    };
    Vector2 aimScreenPosition_ = { 0.0f, 0.0f };
    Vector2 directGamepadAimInput_ = {};
};

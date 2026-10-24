#pragma once

#include <numbers>
#include "Engine/Math/MathStruct.h"
#include <cstdint>
#include <memory>

class DxrRenderer;
class Object3d;
class Player;

// ビーム本体の描画資源、姿勢とアニメーション、接触判定を扱う。
class FearWormBeam {
public:
    FearWormBeam();
    ~FearWormBeam();
    void Initialize();
    void Reset();
    void Stop();
    void ClearHitState();
    void SetDimensions(float length, float width);
    void SetPose(const Vector3& position, const Vector3& direction);
    const Vector3& GetPosition() const { return position_; }
    const Vector3& GetDirection() const { return direction_; }
    float GetLength() const { return length_; }
    float GetWidth() const { return width_; }
    bool IsHittingPlayer() const { return isHittingPlayer_; }
    void UpdateVisual(float deltaTimeSeconds);
    bool IntersectsSphere(const Vector3& spherePosition, float sphereRadius) const;
    void CheckCollision(Player* player);
    void Draw();
    void SubmitRaytracing(DxrRenderer& renderer);

private:
    static constexpr float kUvScrollSpeed = -6.5f;
    static constexpr float kRotateSpeedRadiansPerSecond = 45.0f;
    static constexpr float kPi = std::numbers::pi_v<float>;
    static constexpr float kMinimumDrawLength = 0.001f;
    static constexpr float kPlayerCollisionRadius = 1.0f;
    static constexpr int32_t kDamage = 1;
    static constexpr float kHitEffectIntervalSeconds = 0.05f;

    std::unique_ptr<Object3d> object_;
    Vector3 position_ {};
    Vector3 direction_ = { 0.0f, 0.0f, -1.0f };
    float length_ = 0.0f;
    float width_ = 0.0f;
    float uvScrollOffset_ = 0.0f;
    float rotateAngleRadians_ = 0.0f;
    float hitEffectElapsedSeconds_ = 0.0f;
    bool isHittingPlayer_ = false;
};

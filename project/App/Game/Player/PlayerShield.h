#pragma once
#include "Engine/Math/MathStruct.h"
#include "Engine/Math/Sphere.h"
#include <array>
#include <memory>

class Camera;
class Object3d;

class PlayerShield final {
public:
    PlayerShield();
    ~PlayerShield();

    void Initialize(Camera* camera);
    void Reset();
    void SetCamera(Camera* camera);
    void Update(bool shouldActivate, float deltaTimeSeconds);
    Sphere GetCollisionSphere(const Vector3& playerPosition) const;
    void OnBulletHit(const Vector3& playerPosition, const Vector3& bulletCenter, const Vector3& bulletMovement);
    void Draw(const Vector3& playerPosition);

    bool IsActive() const { return isActive_; }
    float GetChargeRatio() const { return static_cast<float>(remainingSeconds_ / kCapacitySeconds); }

private:
    static constexpr double kCapacitySeconds = 3.0;
    static constexpr float kRadius = 3.6f;
    static constexpr size_t kMaxImpacts = 4;

    struct Impact {
        Vector3 direction_ {};
        float remainingSeconds_ = 0.0f;
    };

    std::unique_ptr<Object3d> shell_;
    Camera* camera_ = nullptr;
    std::array<Impact, kMaxImpacts> impacts_ {};
    size_t nextImpact_ = 0;
    double remainingSeconds_ = kCapacitySeconds;
    float activeSeconds_ = 0.0f;
    float fadeSeconds_ = 0.0f;
    bool isActive_ = false;
};

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
    void OnContactHit(const Vector3& playerPosition);
    void Draw(const Vector3& playerPosition);

    bool IsActive() const { return isActive_; }
    float GetChargeRatio() const { return static_cast<float>(remainingHits_) / kMaxHits; }
    float GetDisplayRatio() const;

private:
    static constexpr int kMaxHits = 4;
    void ConsumeHit(const Vector3& playerPosition);
    static constexpr float kRadius = 3.6f;
    static constexpr size_t kMaxImpacts = 4;
    static constexpr size_t kShardCount = 18;

    struct Impact {
        Vector3 direction_ {};
        float remainingSeconds_ = 0.0f;
    };

    struct BreakShard {
        std::unique_ptr<Object3d> object;
        Vector3 direction {};
        Vector3 rotation {};
        Vector3 spin {};
        float speed = 0.0f;
        float lifetime = 0.0f;
    };

    std::unique_ptr<Object3d> shell_;
    std::array<BreakShard, kShardCount> breakShards_ {};
    Camera* camera_ = nullptr;
    std::array<Impact, kMaxImpacts> impacts_ {};
    size_t nextImpact_ = 0;
    int remainingHits_ = kMaxHits;
    double recoveryDelaySeconds_ = 0.0;
    double recoveryProgressSeconds_ = 0.0;
    float contactCooldownSeconds_ = 0.0f;
    float activeSeconds_ = 0.0f;
    float fadeSeconds_ = 0.0f;
    float breakRemainingSeconds_ = 0.0f;
    Vector3 breakPosition_ {};
    bool isActive_ = false;
    bool needsRelease_ = false;
};

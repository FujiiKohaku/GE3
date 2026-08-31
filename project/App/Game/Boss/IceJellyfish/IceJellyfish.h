#pragma once

#include "Engine/3D/Object3d.h"
#include "Engine/CollisionManager/CollisionManager.h"
#include <array>
#include <memory>

// Passive boss prototype: armor guards bullets; only the core takes damage.
class IceJellyfish {
public:
    void Initialize(Camera* camera, const Vector3& position);
    void Update(float deltaTime, float playerRailZ);
    void UpdateDrawMatrices();
    void Draw();
    SweepHit SweepBullet(const Sphere& bullet, const Vector3& movement, bool& hitCore) const;
    void OnBulletHit(bool hitCore, float damage, const Vector3& position);
    void RegisterRaycastTargets(CollisionObjectId& nextObjectId) const;
    void DrawCollisionDebug() const;
    bool IsDead() const { return hp_ <= 0.0f; }
    float GetHp() const { return hp_; }
    static constexpr float kMaxHp = 120.0f;

private:
    static constexpr size_t kTentacleCount = 6;
    static constexpr size_t kSegmentsPerTentacle = 7;
    static constexpr float kHoverDistance = 150.0f;

    std::unique_ptr<Object3d> CreatePart(
        Camera* camera, const char* modelPath, const Vector4& color, bool ice);

    std::unique_ptr<Object3d> bell_;
    std::unique_ptr<Object3d> core_;
    std::array<std::array<std::unique_ptr<Object3d>, kSegmentsPerTentacle>,
        kTentacleCount> tentacles_;
    Vector3 basePosition_ {};
    float animationTime_ = 0.0f;
    float hp_ = kMaxHp;
    float hitFlashTime_ = 0.0f;
    std::vector<OBB> bellColliders_;
    std::vector<OBB> tentacleColliders_;
    Sphere coreCollider_ {};
};

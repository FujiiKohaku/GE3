#pragma once
#include "Engine/CollisionManager/CollisionManager.h"
#include <cstdint>
#include <vector>

// 敵とボスの部位を、球または箱の当たり判定で表す。
struct EnemyCollisionPart {
    Vector3 position = { 0.0f, 0.0f, 0.0f };
    float radius = 3.0f;
    int32_t partIndex = 0;
    bool isBox = false;
    OBB box {};
    CollisionHit IntersectSphere(const Sphere& sphere) const;
    SweepHit SweepSphere(const Sphere& sphere, const Vector3& movement) const;
    float GetSurfaceDistance(const Vector3& point) const;
};

class CombatTarget {
public:
    virtual ~CombatTarget() = default;
    virtual bool IsDead() const = 0;
    virtual Vector3 GetPosition() const = 0;
    virtual void GetCollisionParts(std::vector<EnemyCollisionPart>& parts) const = 0;
    virtual SweepHit SweepBullet(const Sphere& bullet, const Vector3& movement, int32_t& partIndex) const = 0;
    virtual void OnBulletHit(int32_t partIndex, float damage, const Vector3& position) = 0;
    virtual void RegisterRaycastTargets(CollisionObjectId& nextObjectId) const = 0;
    virtual bool IsCollisionPartDamageable(int32_t partIndex) const = 0;
    virtual bool RequiresBulletOcclusionCheck() const { return false; }
};

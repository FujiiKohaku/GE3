#pragma once
#include "Engine/math/MathStruct.h"
#include <memory>
#include <vector>

class BaseEnemy;
class Camera;

class PlayerHomingLock {
public:
    void SetTargets(const std::vector<BaseEnemy*>& targets);
    bool UpdateFireInput(bool isFireHeld, bool isEnabled, Camera* camera,
        const Vector3& position, const Vector3& forward, const Vector2& aimPosition);
    void ClearTargets() { lockedTargets_.clear(); }
    void Reset();
    void GetPositions(std::vector<Vector3>& positions) const;
    const std::vector<BaseEnemy*>& GetLockedTargets() const { return lockedTargets_; }
    const std::shared_ptr<std::vector<BaseEnemy*>>& GetTargets() const { return targets_; }

private:
    void UpdateTargets(bool isLocking, Camera* camera, const Vector3& position,
        const Vector3& forward, const Vector2& aimPosition);
    std::shared_ptr<std::vector<BaseEnemy*>> targets_ = std::make_shared<std::vector<BaseEnemy*>>();
    std::vector<BaseEnemy*> lockedTargets_;
    bool wasFireHeld_ = false;
    static constexpr float kMaxForwardDistance = 250.0f;
    static constexpr size_t kMaxLockCount = 6;
};

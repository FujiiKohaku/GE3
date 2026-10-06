#include "App/Game/Player/Weapon/PlayerHomingLock.h"
#include "App/Game/Enemy/BaseEnemy.h"
#include "Engine/Camera/Camera.h"
#include "Engine/audio/SoundManager.h"
#include <algorithm>

namespace {
struct LockCandidate {
    BaseEnemy* enemy;
    float screenDistanceSquared;
};

bool CompareLockCandidates(const LockCandidate& left, const LockCandidate& right)
{
    return left.screenDistanceSquared < right.screenDistanceSquared;
}
}

void PlayerHomingLock::SetTargets(const std::vector<BaseEnemy*>& targets)
{
    *targets_ = targets;
    for (auto targetIterator = lockedTargets_.begin(); targetIterator != lockedTargets_.end();) {
        if (std::find(targets.begin(), targets.end(), *targetIterator) == targets.end()) {
            targetIterator = lockedTargets_.erase(targetIterator);
        } else {
            ++targetIterator;
        }
    }
}

void PlayerHomingLock::Reset()
{
    lockedTargets_.clear();
    wasFireHeld_ = false;
}

bool PlayerHomingLock::UpdateFireInput(bool isFireHeld, bool isEnabled, Camera* camera,
    const Vector3& position, const Vector3& forward, const Vector2& aimPosition)
{
    if (!isEnabled) {
        Reset();
        return false;
    }
    if (isFireHeld && !wasFireHeld_) {
        ClearTargets();
    }
    UpdateTargets(isFireHeld, camera, position, forward, aimPosition);
    const bool shouldFire = !isFireHeld && wasFireHeld_ && camera != nullptr;
    wasFireHeld_ = isFireHeld;
    return shouldFire;
}

void PlayerHomingLock::UpdateTargets(bool isLocking, Camera* camera,
    const Vector3& position, const Vector3& forward, const Vector2& aimPosition)
{
    if (!isLocking ||
        camera == nullptr || lockedTargets_.size() >= kMaxLockCount) {
        return;
    }

    const std::size_t previousLockCount = lockedTargets_.size();
    constexpr float kLockRadiusPixels = 120.0f;
    std::vector<LockCandidate> candidates;
    for (BaseEnemy* enemy : *targets_) {
        if (enemy == nullptr || enemy->IsDead()) {
            continue;
        }
        const Vector3 toEnemy = enemy->GetPosition() - position;
        const float forwardDistance = Dot(toEnemy, forward);
        if (forwardDistance <= 0.0f ||
            forwardDistance > kMaxForwardDistance) {
            continue;
        }
        const Vector2 screenPosition = camera->WorldToScreen(enemy->GetPosition());
        const float differenceX = screenPosition.x - aimPosition.x;
        const float differenceY = screenPosition.y - aimPosition.y;
        const float distanceSquared = differenceX * differenceX + differenceY * differenceY;
        if (distanceSquared <= kLockRadiusPixels * kLockRadiusPixels) {
            candidates.push_back({ enemy, distanceSquared });
        }
    }

    std::sort(candidates.begin(), candidates.end(), CompareLockCandidates);
    for (const LockCandidate& candidate : candidates) {
        if (std::find(
                lockedTargets_.begin(),
                lockedTargets_.end(),
                candidate.enemy) != lockedTargets_.end()) {
            continue;
        }
        lockedTargets_.push_back(candidate.enemy);
        if (lockedTargets_.size() >= kMaxLockCount) {
            break;
        }
    }
    if (lockedTargets_.size() > previousLockCount) {
        SoundManager::GetInstance()->Play("HomingLock");
    }
}

void PlayerHomingLock::GetPositions(std::vector<Vector3>& positions) const
{
    positions.clear();
    for (BaseEnemy* target : lockedTargets_) {
        if (target != nullptr && !target->IsDead()) {
            positions.push_back(target->GetPosition());
        }
    }
}

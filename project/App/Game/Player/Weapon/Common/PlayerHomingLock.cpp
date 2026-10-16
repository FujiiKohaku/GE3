// 照準に近い敵をロックし、射撃ボタンを離したタイミングで発射を要求する。
#include "App/Game/Player/Weapon/Common/PlayerHomingLock.h"
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

// 現在の候補一覧を共有データへ反映し、弾が対象の生存を確認できるようにする。
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

// ロック対象と前回の押下状態を消し、次の射撃入力を新しく受け付ける。
void PlayerHomingLock::Reset()
{
    lockedTargets_.clear();
    wasFireHeld_ = false;
}

// 押下中のロック更新と、ボタンを離した瞬間の発射要求を判定する。
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

// 前方距離と画面上の照準との近さを調べ、条件を満たす敵を上限数までロックする。
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

// 生存中のロック対象の位置を集め、画面のロックマーカー表示に使う。
void PlayerHomingLock::GetPositions(std::vector<Vector3>& positions) const
{
    positions.clear();
    for (BaseEnemy* target : lockedTargets_) {
        if (target != nullptr && !target->IsDead()) {
            positions.push_back(target->GetPosition());
        }
    }
}

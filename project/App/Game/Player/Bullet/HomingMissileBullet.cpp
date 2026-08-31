#include "App/Game/Player/Bullet/HomingMissileBullet.h"
#include "App/Game/Enemy/BaseEnemy.h"
#include "Engine/Time/TimeManager.h"
#include <algorithm>
#include <cmath>

void HomingMissileBullet::SetTarget(BaseEnemy* target, const std::shared_ptr<std::vector<BaseEnemy*>>& activeTargets)
{
    target_ = target;
    activeTargets_ = activeTargets;
    targetPassCount_ = 0;
}

void HomingMissileBullet::Move()
{
    const std::shared_ptr<std::vector<BaseEnemy*>> activeTargets = activeTargets_.lock();
    const bool targetIsActive = activeTargets != nullptr &&
        std::find(activeTargets->begin(), activeTargets->end(), target_) != activeTargets->end();

    if (targetIsActive && target_ != nullptr && !target_->IsDead()) {
        const Vector3 targetPosition = target_->GetPosition();
        const Vector3 toTarget = targetPosition - transform_.translate;
        const float distanceSquared = Dot(toTarget, toTarget);
        if (distanceSquared > 0.0001f) {
            const float speed = std::sqrt(Dot(velocity_, velocity_));
            const Vector3 desiredVelocity = Normalize(toTarget) * speed;
            const float frameScale = TimeManager::GetInstance()->GetDeltaTime() * 60.0f;
            const float distance = std::sqrt(distanceSquared);
            const float closeRatio = std::clamp(
                1.0f - distance / kCloseHomingDistance,
                0.0f,
                1.0f);
            const float steeringStrength =
                homingStrength_ +
                (kCloseHomingStrength - homingStrength_) * closeRatio;
            const float blend = std::clamp(steeringStrength * frameScale, 0.0f, 1.0f);
            velocity_ = Normalize(velocity_ * (1.0f - blend) + desiredVelocity * blend) * speed;

            const Vector3 frameMovement = velocity_ * frameScale;
            const Vector3 afterMoveToTarget = toTarget - frameMovement;
            const bool passesTargetThisFrame =
                Dot(toTarget, velocity_) > 0.0f &&
                Dot(afterMoveToTarget, velocity_) <= 0.0f;
            if (passesTargetThisFrame) {
                const float movementSquared = Dot(frameMovement, frameMovement);
                float closestDistanceSquared = distanceSquared;
                if (movementSquared > 0.0001f) {
                    const float closestTime = std::clamp(
                        Dot(toTarget, frameMovement) / movementSquared,
                        0.0f,
                        1.0f);
                    const Vector3 closestOffset =
                        toTarget - frameMovement * closestTime;
                    closestDistanceSquared = Dot(closestOffset, closestOffset);
                }

                if (closestDistanceSquared <=
                    kTargetSnapDistance * kTargetSnapDistance) {
                    transform_.translate = targetPosition;
                    return;
                }

                ++targetPassCount_;
                if (targetPassCount_ >= kMaxTargetPassCount) {
                    SetDead();
                    return;
                }
            }
        }
    } else {
        target_ = nullptr;
    }

    MissileBullet::Move();
}

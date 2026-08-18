#include "App/Game/Player/Bullet/HomingMissileBullet.h"
#include "App/Game/Enemy/BaseEnemy.h"
#include "Engine/Time/TimeManager.h"
#include <algorithm>
#include <cmath>

void HomingMissileBullet::SetTarget(BaseEnemy* target, const std::shared_ptr<std::vector<BaseEnemy*>>& activeTargets)
{
    target_ = target;
    activeTargets_ = activeTargets;
}

void HomingMissileBullet::Move()
{
    const std::shared_ptr<std::vector<BaseEnemy*>> activeTargets = activeTargets_.lock();
    const bool targetIsActive = activeTargets != nullptr &&
        std::find(activeTargets->begin(), activeTargets->end(), target_) != activeTargets->end();

    if (targetIsActive && target_ != nullptr && !target_->IsDead()) {
        const Vector3 toTarget = target_->GetPosition() - transform_.translate;
        if (Dot(toTarget, toTarget) > 0.0001f) {
            const float speed = std::sqrt(Dot(velocity_, velocity_));
            const Vector3 desiredVelocity = Normalize(toTarget) * speed;
            const float frameScale = TimeManager::GetInstance()->GetDeltaTime() * 60.0f;
            const float blend = std::clamp(homingStrength_ * frameScale, 0.0f, 1.0f);
            velocity_ = Normalize(velocity_ * (1.0f - blend) + desiredVelocity * blend) * speed;
        }
    } else {
        target_ = nullptr;
    }

    MissileBullet::Move();
}

#include "App/Game/Enemy/Bullet/EnemyBullet.h"

#include <algorithm>

float EnemyBullet::timeScale_ = 1.0f;

void EnemyBullet::Initialize(Model* model)
{
    BaseBullet::Initialize(model);
}

void EnemyBullet::OnHitPlayer(const Vector3&)
{
    justDodgeResolved_ = true;
}

void EnemyBullet::SetTimeScale(float timeScale)
{
    timeScale_ = std::clamp(timeScale, 0.0f, 1.0f);
}

float EnemyBullet::GetCurrentTimeScale()
{
    return timeScale_;
}

void EnemyBullet::MarkJustDodgeCandidate()
{
    if (!justDodgeResolved_) {
        justDodgeCandidate_ = true;
    }
}

bool EnemyBullet::ResolveJustDodge()
{
    if (!justDodgeCandidate_ || justDodgeResolved_) {
        return false;
    }
    justDodgeResolved_ = true;
    return true;
}

float EnemyBullet::GetTimeScale() const
{
    return timeScale_;
}

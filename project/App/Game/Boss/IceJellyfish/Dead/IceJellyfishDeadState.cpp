#include "IceJellyfishDeadState.h"
#include "../IceJellyfish.h"
#include "Engine/Time/TimeManager.h"

void IceJellyfishDeadState::Reset()
{
    elapsedSeconds_ = 0.0f;
}

void IceJellyfishDeadState::Enter(IceJellyfish&)
{
    Reset();
}

void IceJellyfishDeadState::Update(IceJellyfish& boss)
{
    const float deltaTimeSeconds = TimeManager::GetInstance()->GetDeltaTime();
    elapsedSeconds_ += deltaTimeSeconds;
    boss.bodyPosition_.y -= kFallSpeedUnitsPerSecond * deltaTimeSeconds;
    boss.bodyPosition_.z += kForwardSpeedUnitsPerSecond * deltaTimeSeconds;
}

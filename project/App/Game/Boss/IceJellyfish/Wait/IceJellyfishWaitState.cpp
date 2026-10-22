#include "IceJellyfishWaitState.h"
#include "../IceJellyfish.h"

void IceJellyfishWaitState::Update(IceJellyfish& boss)
{
    boss.ChangeState(IceJellyfish::BossState::Entry);
}

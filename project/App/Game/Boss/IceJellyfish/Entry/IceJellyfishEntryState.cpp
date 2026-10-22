#include "IceJellyfishEntryState.h"
#include "../IceJellyfish.h"

void IceJellyfishEntryState::Update(IceJellyfish& boss)
{
    boss.ChangeState(IceJellyfish::BossState::Battle);
}

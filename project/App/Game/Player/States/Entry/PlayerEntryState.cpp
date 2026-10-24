#include "PlayerEntryState.h"
#include "App/Game/Player/Player.h"

void PlayerEntryState::Update(Player& player)
{
    player.ChangeState(Player::State::Battle);
}

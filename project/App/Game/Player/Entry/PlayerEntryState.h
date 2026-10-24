#pragma once

class Player;

// Player固有の登場演出はまだないため、待ち時間を加えず戦闘へ進む。
class PlayerEntryState {
public:
    void Update(Player& player);
};

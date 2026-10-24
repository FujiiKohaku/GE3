#pragma once

class Player;

// 通常操作中の照準・移動・射撃の更新順序を管理する。
class PlayerBattleState {
public:
    void Update(Player& player);
};

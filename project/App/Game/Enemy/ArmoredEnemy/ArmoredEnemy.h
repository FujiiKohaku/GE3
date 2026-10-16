// 通常敵をもとにHPと攻撃設定を変えた、耐久力の高い敵を定義する。
#pragma once

#include "App/Game/Enemy/NormalEnemy/NormalEnemy.h"

class Model;
class Player;

class ArmoredEnemy : public NormalEnemy {
public:
    std::string_view GetAppearanceId() const override { return "ArmoredEnemy"; }
    // 通常敵の初期化を行った後、装甲敵用の耐久力と攻撃設定へ変更する。
    void Initialize(
        Model* model,
        Model* bulletModel,
        Player* player);
};

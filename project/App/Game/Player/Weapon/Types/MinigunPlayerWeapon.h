// 一発の威力を抑え、短い間隔で連射するミニガンの設定を定義する。
#pragma once
#include "App/Game/Player/Weapon/Common/BasePlayerWeapon.h"

class MinigunPlayerWeapon final : public AutomaticPlayerWeapon {
public:
    // 通常弾より短い発射間隔と低い基本威力を、共通の連射処理へ渡す。
    MinigunPlayerWeapon();
    const char* GetName() const override;
};
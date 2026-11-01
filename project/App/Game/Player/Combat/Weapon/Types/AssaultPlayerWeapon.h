// 短い間隔で連射するアサルトの設定を定義する。
#pragma once
#include "App/Game/Player/Combat/Weapon/Common/BasePlayerWeapon.h"

class AssaultPlayerWeapon final : public AutomaticPlayerWeapon {
public:
    // 発射間隔・基本威力・効果音を共通の連射処理へ渡す。
    AssaultPlayerWeapon();
    const char* GetName() const override;
};

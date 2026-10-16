// 標準の威力・発射間隔で連射する、通常弾の武器設定を定義する。
#pragma once
#include "App/Game/Player/Weapon/Common/BasePlayerWeapon.h"

class NormalPlayerWeapon final : public AutomaticPlayerWeapon {
public:
    // 通常弾の連射間隔・基本威力・発熱区分・効果音を共通の連射処理へ渡す。
    NormalPlayerWeapon();
    const char* GetName() const override;
};
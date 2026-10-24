// 標準の威力・発射間隔で連射する、通常弾の武器設定を定義する。
#include "App/Game/Player/Combat/Weapon/Types/NormalPlayerWeapon.h"

// 通常弾の連射間隔・基本威力・発熱区分・効果音を共通の連射処理へ渡す。
NormalPlayerWeapon::NormalPlayerWeapon() : AutomaticPlayerWeapon(10.0f / 60.0f, 3, false, "NormalShot") {}

const char* NormalPlayerWeapon::GetName() const
{
    return "Normal";
}
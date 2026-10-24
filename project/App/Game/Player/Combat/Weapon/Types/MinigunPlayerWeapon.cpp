// 一発の威力を抑え、短い間隔で連射するミニガンの設定を定義する。
#include "App/Game/Player/Combat/Weapon/Types/MinigunPlayerWeapon.h"

// 通常弾より短い発射間隔と低い基本威力を、共通の連射処理へ渡す。
MinigunPlayerWeapon::MinigunPlayerWeapon() : AutomaticPlayerWeapon(3.0f / 60.0f, 1, true, "MinigunShot") {}

const char* MinigunPlayerWeapon::GetName() const
{
    return "Minigun";
}
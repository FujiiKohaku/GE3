// 短い間隔で連射するアサルトの設定を定義する。
#include "App/Game/Player/Combat/Weapon/Types/AssaultPlayerWeapon.h"

// 旧ミニガンの連射性能を引き継ぐ。
AssaultPlayerWeapon::AssaultPlayerWeapon() : AutomaticPlayerWeapon(3.0f / 60.0f, 1, "AssaultShot") {}

const char* AssaultPlayerWeapon::GetName() const
{
    return "アサルト";
}

#pragma once
#include "App/Game/Player/Weapon/PlayerWeaponController.h"
#include "App/Game/Player/Weapon/Types/HomingPlayerWeapon.h"
#include "App/Game/Player/Weapon/Types/MissilePlayerWeapon.h"
#include "App/Game/Player/Weapon/Types/MinigunPlayerWeapon.h"
#include "App/Game/Player/Weapon/Types/NormalPlayerWeapon.h"
#include "App/Game/Player/Weapon/Common/BasePlayerWeapon.h"

// 描画を起動せず、基底ポインタ経由の発射タイミングと発射制限を検証する。
inline bool RunPlayerWeaponPolymorphismTests()
{
    PlayerWeaponHeat heat;
    PlayerHomingLock lock;
    float missileCooldown = 0.5f;
    int shots = 0;
    PlayerWeaponContext context { heat, lock, missileCooldown, nullptr, {}, {}, {}, true,
        [&shots]() { ++shots; return true; }, [](BaseEnemy*) {} };
    NormalPlayerWeapon normal;
    BasePlayerWeapon* weapon = &normal;
    weapon->Update(context, true, true, 0.0f);
    weapon->Update(context, true, false, 0.5f);
    if (shots != 4) return false; // 初弾1発＋0.5秒で3発。
    weapon->Update(context, false, false, 1.0f);
    if (shots != 4) return false;
    context.canFire = false;
    weapon->Update(context, true, true, 1.0f);
    if (shots != 4 || weapon->Fire(context)) return false;

    context.canFire = true;
    shots = 0;
    MinigunPlayerWeapon minigun;
    weapon = &minigun;
    weapon->Update(context, true, true, 0.1f);
    if (shots != 3) return false; // 初弾1発＋0.1秒で2発。

    shots = 0;
    MissilePlayerWeapon missile;
    weapon = &missile;
    weapon->Update(context, true, false, 1.0f);
    if (shots != 0) return false;
    weapon->Update(context, true, true, 0.0f);
    if (shots != 1 || weapon->Fire(context)) return false;
    HomingPlayerWeapon homing;
    weapon = &homing;
    if (weapon->Fire(context)) return false; // 同じミサイルクールダウンを共有。
    context.canFire = false;
    weapon->Update(context, true, true, 1.0f);
    if (shots != 1) return false;

    context.canFire = true;
    for (int i = 0; i < 20; ++i) heat.AddShotHeat(false);
    heat.Update(0.5f, true); // 熱100%到達後の猶予時間を過ぎると過熱する。
    return heat.IsOverheated() && !normal.Fire(context) && !minigun.Fire(context);
}

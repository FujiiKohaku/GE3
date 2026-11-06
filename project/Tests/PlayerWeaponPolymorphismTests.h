#pragma once
#include "App/Game/Player/Combat/Weapon/PlayerWeaponController.h"
#include "App/Game/Player/Combat/Weapon/Types/HomingPlayerWeapon.h"
#include "App/Game/Player/Combat/Weapon/Types/MissilePlayerWeapon.h"
#include "App/Game/Player/Combat/Weapon/Types/AssaultPlayerWeapon.h"
#include "App/Game/Player/Combat/Weapon/Common/BasePlayerWeapon.h"
#include <cstring>
#include <cmath>

namespace PlayerWeaponTest {
inline bool CountShot(int& shots)
{
    ++shots;
    return true;
}
inline void IgnoreTarget(BaseEnemy*) {}
inline void CountEmission(int& shots, BaseEnemy*) { ++shots; }
}

// 描画を起動せず、基底ポインタ経由の発射タイミングと発射制限を検証する。
inline bool RunPlayerWeaponPolymorphismTests()
{
    PlayerWeaponHeat heat;
    PlayerHomingLock lock;
    int shots = 0;
    PlayerWeaponContext context { heat, lock, nullptr, {}, {}, {}, true,
        std::bind(PlayerWeaponTest::CountShot, std::ref(shots)), PlayerWeaponTest::IgnoreTarget };
    AssaultPlayerWeapon assault;
    BasePlayerWeapon* weapon = &assault;
    if (std::strcmp(weapon->GetName(), "アサルト") != 0 || !weapon->UsesHeat()) { return false; }
    weapon->Update(context, true, true, 0.0f);
    weapon->Update(context, true, false, 0.5f);
    if (shots != 11) return false; // 初弾1発＋0.5秒で10発。
    weapon->Update(context, false, false, 1.0f);
    if (shots != 11) return false;
    context.canFire = false;
    weapon->Update(context, true, true, 1.0f);
    if (shots != 11 || weapon->Fire(context)) return false;

    context.canFire = true;
    shots = 0;
    weapon->Update(context, true, true, 0.1f);
    if (shots != 3) return false; // 初弾1発＋0.1秒で2発。

    shots = 0;
    MissilePlayerWeapon missile;
    weapon = &missile;
    context.fire = std::bind(&BasePlayerWeapon::Fire, weapon, std::ref(context));
    context.emit = std::bind(PlayerWeaponTest::CountEmission, std::ref(shots), std::placeholders::_1);
    weapon->Update(context, true, false, 1.0f);
    if (shots != 0) return false;
    weapon->Update(context, true, true, 0.0f);
    if (shots != 1 || weapon->Fire(context)) return false;
    HomingPlayerWeapon homing;
    weapon = &homing;
    if (!weapon->Fire(context)) return false; // 通常ミサイルとは待ち時間を共有しない。
    if (shots != 2 || weapon->Fire(context)) return false;
    if (missile.GetFireIntervalSeconds() != 2.0f || homing.GetFireIntervalSeconds() != 4.0f) return false;
    missile.UpdateCooldown(2.0f);
    homing.UpdateCooldown(2.0f);
    if (!missile.IsReadyToFire() || homing.IsReadyToFire()) return false;
    if (std::abs(homing.GetReloadRatio() - 0.5f) > 0.0001f) return false;
    if (!missile.Fire(context) || homing.Fire(context)) return false;
    homing.UpdateCooldown(2.0f);
    if (!homing.IsReadyToFire() || !homing.Fire(context)) return false;
    if (missile.IsReadyToFire()) return false;
    context.canFire = false;
    const int shotsBeforeBlockedFire = shots;
    weapon->Update(context, true, true, 1.0f);
    if (shots != shotsBeforeBlockedFire) return false;

    for (int framesPerSecond : { 30, 60, 120 }) {
        MissilePlayerWeapon timedMissile;
        HomingPlayerWeapon timedHoming;
        context.canFire = true;
        if (!timedMissile.Fire(context) || !timedHoming.Fire(context)) return false;
        const float deltaTimeSeconds = 1.0f / static_cast<float>(framesPerSecond);
        for (int frame = 0; frame < framesPerSecond * 4; ++frame) {
            timedMissile.UpdateCooldown(deltaTimeSeconds);
            timedHoming.UpdateCooldown(deltaTimeSeconds);
            if (frame == framesPerSecond) {
                if (timedMissile.IsReadyToFire() || timedHoming.IsReadyToFire()) return false;
            }
            if (frame == framesPerSecond * 2) {
                if (!timedMissile.IsReadyToFire() || timedHoming.IsReadyToFire()) return false;
            }
        }
        if (!timedMissile.IsReadyToFire() || !timedHoming.IsReadyToFire()) return false;
        if (timedMissile.GetReloadRatio() < 0.9999f || timedHoming.GetReloadRatio() < 0.9999f) return false;
    }

    context.canFire = true;
    constexpr int kMaxShots = 1000;
    for (int shotIndex = 0; shotIndex < kMaxShots && heat.GetHeatRatio() < 1.0f; ++shotIndex) { heat.AddShotHeat(); }
    heat.Update(0.5f, true); // 熱100%到達後の猶予時間を過ぎると過熱する。
    return heat.IsOverheated() && !assault.Fire(context);
}

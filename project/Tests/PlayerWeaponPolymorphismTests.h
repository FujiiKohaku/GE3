#pragma once
#include "App/Game/Player/Player.h"
#include <cstring>

// 武器の発射数と、ミサイルごとの待ち時間を検証する。
inline bool RunPlayerWeaponPolymorphismTests()
{
    Player player;
    PlayerWeaponController controller(player);
    if (std::strcmp(controller.GetCurrentWeaponName(), "アサルト") != 0) { return false; }
    int shots = controller.GetRequestedShotCount(true, true, 0.0f);
    shots += controller.GetRequestedShotCount(true, false, 0.5f);
    if (shots != 11) { return false; }
    if (controller.GetRequestedShotCount(false, false, 1.0f) != 0) { return false; }
    if (controller.GetRequestedShotCount(true, true, 0.1f) != 3) { return false; }
    controller.currentWeapon_ = controller.kWeaponMissileBullet;
    if (controller.GetRequestedShotCount(true, false, 1.0f) != 0) { return false; }
    if (controller.GetRequestedShotCount(true, true, 0.0f) != 1) { return false; }
    if (!controller.TryStartCooldown() || controller.TryStartCooldown()) { return false; }
    controller.currentWeapon_ = controller.kWeaponHomingMissile;
    if (!controller.TryStartCooldown() || controller.TryStartCooldown()) { return false; }
    controller.UpdateCooldowns(1.0f);
    if (controller.GetMissileReloadRatio() != 0.25f || controller.IsWeaponReady()) { return false; }
    controller.currentWeapon_ = controller.kWeaponMissileBullet;
    if (controller.GetMissileReloadRatio() != 0.5f || controller.IsWeaponReady()) { return false; }
    controller.UpdateCooldowns(1.0f);
    controller.currentWeapon_ = controller.kWeaponHomingMissile;
    if (controller.IsWeaponReady()) { return false; }
    controller.currentWeapon_ = controller.kWeaponMissileBullet;
    if (!controller.IsWeaponReady() || !controller.TryStartCooldown()) { return false; }
    controller.UpdateCooldowns(2.0f);
    controller.currentWeapon_ = controller.kWeaponHomingMissile;
    if (!controller.TryStartCooldown()) { return false; }
    for (int framesPerSecond : { 30, 60, 120 }) {
        PlayerWeaponController timedController(player);
        timedController.currentWeapon_ = timedController.kWeaponMissileBullet;
        if (!timedController.TryStartCooldown()) { return false; }
        timedController.currentWeapon_ = timedController.kWeaponHomingMissile;
        if (!timedController.TryStartCooldown()) { return false; }
        const float deltaTimeSeconds = 1.0f / static_cast<float>(framesPerSecond);
        for (int frame = 0; frame < framesPerSecond * 4; ++frame) {
            timedController.UpdateCooldowns(deltaTimeSeconds);
        }
        if (!timedController.IsWeaponReady()) { return false; }
        timedController.currentWeapon_ = timedController.kWeaponMissileBullet;
        if (!timedController.IsWeaponReady()) { return false; }
    }
    return true;
}

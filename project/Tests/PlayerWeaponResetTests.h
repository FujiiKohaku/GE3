#pragma once

#include "App/Game/Player/Player.h"
#include "Engine/Camera/Camera.h"
#include <cassert>

class PlayerWeaponResetBulletProbe final : public PlayerBullet {
public:
    explicit PlayerWeaponResetBulletProbe(bool& stopped, bool& destroyed)
        : stopped_(stopped), destroyed_(destroyed) {}
    void SetDead() override { stopped_ = true; }
    ~PlayerWeaponResetBulletProbe() override { destroyed_ = true; }
private:
    bool& stopped_;
    bool& destroyed_;
};

inline bool RunPlayerWeaponResetTests()
{
    Player player;
    PlayerWeaponController controller(player);
    Camera camera;
    controller.SetCamera(&camera);
    bool stopped = false;
    bool destroyed = false;
    controller.bullets_.push_back(std::make_unique<PlayerWeaponResetBulletProbe>(stopped, destroyed));
    controller.currentWeapon_ = controller.kWeaponMinigun;
    controller.shotsFired_ = 100;
    controller.missileFireCooldownSeconds_ = 0.0f;
    for (int index = 0; index < 40; ++index) controller.weaponHeat_.AddShotHeat(false);
    controller.weaponHeat_.Update(1.0f, true);
    assert(controller.IsOverheated());
    controller.homingLock_.SetTargets({ nullptr });
    const auto sharedTargets = controller.homingLock_.GetTargets();
    controller.homingLock_.UpdateFireInput(true, true, nullptr, {}, {}, {});

    // 通常弾と非選択のミニガンの両方に、連射間隔の途中の状態を作る。
    int shots = 0;
    auto context = controller.MakeWeaponContext();
    context.canFire = true;
    context.fire = [&shots]() { ++shots; return true; };
    controller.weapons_[controller.kWeaponNormalBullet]->Update(context, true, false, 0.10f);
    controller.weapons_[controller.kWeaponMinigun]->Update(context, true, false, 0.04f);
    assert(shots == 0);

    // 描画初期化なしで、Initializeと同じ戦闘状態リセットを検証する。
    controller.ResetCombatState();
    assert(stopped && destroyed && controller.GetBullets().empty());
    assert(controller.currentWeapon_ == controller.kWeaponNormalBullet && controller.GetShotsFired() == 0);
    assert(controller.GetHeatRatio() == 0.0f && !controller.IsOverheated());
    assert(controller.missileFireCooldownSeconds_ == controller.kMissileFireIntervalSeconds);
    assert(controller.camera_ == &camera && sharedTargets->empty());
    assert(!controller.homingLock_.UpdateFireInput(false, true, &camera, {}, {}, {}));
    auto resetContext = controller.MakeWeaponContext();
    resetContext.canFire = true;
    resetContext.fire = [&shots]() { ++shots; return true; };
    controller.weapons_[controller.kWeaponNormalBullet]->Update(resetContext, true, false, 0.08f);
    controller.weapons_[controller.kWeaponMinigun]->Update(resetContext, true, false, 0.02f);
    assert(shots == 0);
    controller.ResetCombatState(); // 繰り返しても安全。
    return true;
}

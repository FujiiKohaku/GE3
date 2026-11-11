#pragma once

#include "App/Game/Player/Player.h"
#include "Engine/Camera/Camera.h"
#include "Tests/PlayerWeaponPolymorphismTests.h"
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
    controller.currentWeapon_ = controller.kWeaponHomingMissile;
    controller.shotsFired_ = 100;
    controller.currentWeapon_ = controller.kWeaponMissileBullet;
    assert(controller.TryStartCooldown());
    controller.currentWeapon_ = controller.kWeaponHomingMissile;
    assert(controller.TryStartCooldown());
    assert(controller.GetMissileReloadRatio() == 0.0f);
    constexpr int kMaxShots = 1000;
    for (int index = 0; index < kMaxShots && controller.GetHeatRatio() < 1.0f; ++index) {
        controller.AddShotHeat();
    }
    controller.UpdateHeat(1.0f, true);
    assert(controller.IsOverheated());
    controller.SetHomingTargets({ nullptr });
    const auto sharedTargets = controller.homingTargets_;
    controller.UpdateHomingFireInput(true, true, nullptr, {}, {}, {});

    // 非選択のアサルトに、連射間隔の途中の状態を作る。
    controller.currentWeapon_ = controller.kWeaponAssault;
    int shots = controller.GetRequestedShotCount(true, false, 0.04f);
    assert(shots == 0);

    // ボタンを離すまで禁止し、フォーカス喪失後も意図せず発射しない。
    assert(controller.UpdateFireInputState(true, true, 0));
    assert(!controller.UpdateFireInputState(true, true, 0));
    controller.BlockFireUntilRelease();
    assert(!controller.UpdateFireInputState(true, true, 0));
    assert(!controller.UpdateFireInputState(false, true, 0));
    assert(controller.UpdateFireInputState(true, true, 0));
    assert(!controller.UpdateFireInputState(false, false, 1));
    assert(!controller.UpdateFireInputState(true, true, 1));
    assert(!controller.UpdateFireInputState(false, true, 1));
    assert(controller.UpdateFireInputState(true, true, 1));
    // 更新を停止中にフォーカスが失われても、復帰後に禁止する。
    assert(!controller.UpdateFireInputState(true, true, 2));
    assert(!controller.UpdateFireInputState(false, true, 2));
    assert(controller.UpdateFireInputState(true, true, 2));
    // 描画初期化なしで、Initializeと同じ戦闘状態リセットを検証する。
    controller.ResetCombatState();
    assert(stopped && destroyed && controller.GetBullets().empty());
    assert(controller.currentWeapon_ == controller.kWeaponAssault && controller.GetShotsFired() == 0);
    assert(std::strcmp(controller.GetCurrentWeaponName(), "アサルト") == 0);
    assert(controller.GetHeatRatio() == 0.0f && !controller.IsOverheated());
    controller.currentWeapon_ = controller.kWeaponMissileBullet;
    assert(controller.GetMissileReloadRatio() == 1.0f);
    controller.currentWeapon_ = controller.kWeaponHomingMissile;
    assert(controller.GetMissileReloadRatio() == 1.0f);
    assert(controller.camera_ == &camera && sharedTargets->empty());
    assert(!controller.UpdateHomingFireInput(false, true, &camera, {}, {}, {}));
    controller.currentWeapon_ = controller.kWeaponAssault;
    shots = controller.GetRequestedShotCount(true, false, 0.02f);
    assert(shots == 0);
    controller.ResetCombatState(); // 繰り返しても安全。
    return true;
}

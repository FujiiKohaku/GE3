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
    NormalBullet assaultBullet;
    assaultBullet.SetDamage(1);
    assert(assaultBullet.GetHitDamage() == 0.5f);
    assaultBullet.SetDamage(2);
    assert(assaultBullet.GetHitDamage() == 1.0f);
    MissileBullet missileBullet;
    missileBullet.SetDamage(10);
    assert(missileBullet.GetHitDamage() == 10.0f);
    Player player;
    player.SetScale({ 1.0f, 1.0f, 1.0f });
    player.SetRotate({ 0.0f, 0.0f, 0.0f });
    player.SetTranslate({ 0.0f, 0.0f, 0.0f });
    PlayerWeaponController controller(player);
    Camera camera;
    controller.SetCamera(&camera);
    bool stopped = false;
    bool destroyed = false;
    controller.bullets_.push_back(std::make_unique<PlayerWeaponResetBulletProbe>(stopped, destroyed));
    controller.currentWeapon_ = controller.kWeaponHomingMissile;
    controller.shotsFired_ = 100;
    controller.currentWeapon_ = controller.kWeaponMissileBullet;
    assert(controller.IsWeaponReady());
    controller.StartCooldown();
    controller.currentWeapon_ = controller.kWeaponHomingMissile;
    assert(controller.IsWeaponReady());
    controller.StartCooldown();
    assert(controller.GetMissileReloadRatio() == 0.0f);
    constexpr int kMaxShots = 1000;
    for (int index = 0; index < kMaxShots && controller.GetHeatRatio() < 1.0f; ++index) {
        controller.AddShotHeat();
    }
    controller.UpdateAssaultHeat(1.0f, true);
    assert(controller.IsOverheated());
    controller.SetHomingTargets({ nullptr });
    const auto sharedTargets = controller.homingTargets_;
    controller.UpdateHomingFireInput(true, true, nullptr, {}, {}, {});

    // 非選択のアサルトに、連射間隔の途中の状態を作る。
    controller.currentWeapon_ = controller.kWeaponAssault;
    const Vector3 leftMuzzle = controller.CalculateShotMuzzlePosition();
    controller.isNextAssaultShotLeft_ = false;
    const Vector3 rightMuzzle = controller.CalculateShotMuzzlePosition();
    assert(leftMuzzle.x < rightMuzzle.x);
    assert(leftMuzzle.y == rightMuzzle.y && leftMuzzle.z == rightMuzzle.z);
    int shots = controller.GetRequestedShotCount(true, false, 0.04f);
    assert(shots == 0);

    // メニュー操作後の入力禁止は、戦闘状態のリセットで解除する。
    controller.BlockFireUntilRelease();
    assert(controller.isFireBlocked_);
    // 描画初期化なしで、Initializeと同じ戦闘状態リセットを検証する。
    controller.ResetCombatState();
    assert(!controller.isFireBlocked_);
    assert(stopped && destroyed && controller.GetBullets().empty());
    assert(controller.currentWeapon_ == controller.kWeaponAssault && controller.GetShotsFired() == 0);
    assert(controller.isNextAssaultShotLeft_);
    assert(controller.CalculateShotMuzzlePosition().x == leftMuzzle.x);
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

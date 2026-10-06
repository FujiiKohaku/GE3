#pragma once
#include "App/Game/Player/Bullet/PlayerBullet.h"
#include "App/Game/Player/Weapon/PlayerWeaponHeat.h"
#include "App/Game/Player/Weapon/PlayerHomingLock.h"
#include "Engine/math/MathStruct.h"
#include <memory>
#include <vector>

class Model;
class Camera;
class Input;
class PlayerAimController;
class PlayerMovementController;
class PlayerHealth;

class PlayerWeaponController {
public:
    PlayerWeaponController(const EulerTransform& transform, const PlayerAimController& aimController,
        const PlayerMovementController& movementController, const PlayerHealth& health)
        : transform_(transform), aimController_(aimController),
          movementController_(movementController), health_(health) {}
    void Initialize();
    void SetCamera(Camera* camera);
    void UpdateInput(Input* input, bool isDebugMode);
    void FireBullet(const Camera& activeCamera);
    void UpdateBullets();
    void RemoveDeadBullets();
    void DrawBullets();
    const std::vector<std::unique_ptr<PlayerBullet>>& GetBullets() const { return bullets_; }
    void SetHomingTargets(const std::vector<BaseEnemy*>& targets) { homingLock_.SetTargets(targets); }
    void GetHomingLockPositions(std::vector<Vector3>& positions) const;
    void ResetHomingLock() { homingLock_.Reset(); }
    bool IsHomingMissileSelected() const { return currentWeapon_ == kWeaponHomingMissile; }
    const char* GetCurrentWeaponName() const;
    float GetHeatRatio() const { return weaponHeat_.GetHeatRatio(); }
    bool IsOverheated() const { return weaponHeat_.IsOverheated(); }
#ifdef _DEBUG
    void DrawAimDebugLines();
#endif

private:
    enum WeaponType {
        kWeaponNormalBullet = 0,
        kWeaponMissileBullet,
        kWeaponHomingMissile,
        kWeaponMinigun,
        kWeaponCount
    };
    void UpdateWeaponSwitch(Input* input);
    void UpdateHomingFireInput(bool isFireHeld, bool isDebugMode);
    void UpdateWeaponHeat(bool isFireHeld, bool isDebugMode);
    void UpdateFireInput(Input* input, bool isDebugMode);
    Vector3 CalculateMuzzlePosition() const;
    std::unique_ptr<PlayerBullet> CreateBullet(float& shotSpeed);
    void FireSingleBullet(const Camera& activeCamera, BaseEnemy* homingTarget);

    const EulerTransform& transform_;
    const PlayerAimController& aimController_;
    const PlayerMovementController& movementController_;
    const PlayerHealth& health_;
    Model* bulletModel_ = nullptr;
    Camera* camera_ = nullptr;
    std::vector<std::unique_ptr<PlayerBullet>> bullets_;
    PlayerHomingLock homingLock_;
    PlayerWeaponHeat weaponHeat_;
    float bulletSpawnOffsetY_ = 0.3f;
    float bulletSpawnOffsetZ_ = 4.0f;
    float bulletSpeed_ = 5.8f;
    int currentWeapon_ = kWeaponNormalBullet;
    static constexpr int kNormalBulletDamage = 3;
    static constexpr int kNormalFireIntervalFrames = 10;
    int normalFireCooldownFrames_ = 0;
    static constexpr int kMissileFireIntervalFrames = 120;
    int missileFireCooldownFrames_ = kMissileFireIntervalFrames;
    static constexpr int kMinigunDamage = 1;
    static constexpr int kMinigunFireIntervalFrames = 3;
    int minigunFireCooldownFrames_ = 0;
#ifdef _DEBUG
    bool shouldDrawDebugLines_ = false;
    Vector3 debugAimRayOrigin_ = {};
    Vector3 debugAimPoint_ = {};
    Vector3 debugMuzzlePosition_ = {};
    Vector3 debugDrawRayOrigin_ = {};
    Vector3 debugDrawAimPoint_ = {};
#endif
};

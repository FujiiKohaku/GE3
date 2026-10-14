#pragma once
#include "App/Game/Player/Bullet/PlayerBullet.h"
#include "App/Game/Player/Weapon/PlayerWeaponHeat.h"
#include "App/Game/Player/Weapon/PlayerHomingLock.h"
#include "App/Game/Player/Weapon/BasePlayerWeapon.h"
#include <array>
#include "Engine/math/MathStruct.h"
#include <memory>
#include <cstdint>
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
    bool FireBullet(const Camera& activeCamera);
    void UpdateBullets();
    void RemoveDeadBullets();
    void DrawBullets();
    void SubmitRaytracing(DxrRenderer& renderer);
    const std::vector<std::unique_ptr<PlayerBullet>>& GetBullets() const { return bullets_; }
    uint64_t GetShotsFired() const { return shotsFired_; }
    void SetHomingTargets(const std::vector<BaseEnemy*>& targets) { homingLock_.SetTargets(targets); }
    void GetHomingLockPositions(std::vector<Vector3>& positions) const;
    void ResetHomingLock() { homingLock_.Reset(); }
    bool IsHomingMissileSelected() const { return GetCurrentWeapon().IsHoming(); }
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
    BasePlayerWeapon& GetCurrentWeapon() const { return *weapons_[currentWeapon_]; }
    PlayerWeaponContext MakeWeaponContext(const Camera* activeCamera = nullptr);
    Vector3 CalculateMuzzlePosition() const;
    void FireSingleBullet(const Camera& activeCamera, BaseEnemy* homingTarget);

    const EulerTransform& transform_;
    const PlayerAimController& aimController_;
    const PlayerMovementController& movementController_;
    const PlayerHealth& health_;
    Model* bulletModel_ = nullptr;
    Camera* camera_ = nullptr;
    std::vector<std::unique_ptr<PlayerBullet>> bullets_;
    uint64_t shotsFired_ = 0;
    PlayerHomingLock homingLock_;
    PlayerWeaponHeat weaponHeat_;
    float bulletSpawnOffsetY_ = 0.3f;
    float bulletSpawnOffsetZ_ = 4.0f;
    float bulletSpeed_ = 5.8f;
    int currentWeapon_ = kWeaponNormalBullet;
    std::array<std::unique_ptr<BasePlayerWeapon>, kWeaponCount> weapons_ {
        std::make_unique<NormalPlayerWeapon>(), std::make_unique<MissilePlayerWeapon>(),
        std::make_unique<HomingPlayerWeapon>(), std::make_unique<MinigunPlayerWeapon>() };
    static constexpr float kMissileFireIntervalSeconds = 2.0f;
    float missileFireCooldownSeconds_ = kMissileFireIntervalSeconds;
#ifdef _DEBUG
    bool shouldDrawDebugLines_ = false;
    Vector3 debugAimRayOrigin_ = {};
    Vector3 debugAimPoint_ = {};
    Vector3 debugMuzzlePosition_ = {};
    Vector3 debugDrawRayOrigin_ = {};
    Vector3 debugDrawAimPoint_ = {};
#endif
};

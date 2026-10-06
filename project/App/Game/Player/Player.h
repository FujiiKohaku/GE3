#pragma once
#include "Engine/3D/Model.h"
#include "Engine/3D/Object3d.h"
#include "Engine/debugcamera/DebugCameraController.h"
#include "Engine/math/MathStruct.h"
#include "App/Game/Player/Aim/PlayerAimController.h"
#include "App/Game/Player/Movement/PlayerMovementController.h"
#include "App/Game/Player/Health/PlayerHealth.h"
#include "App/Game/Player/Weapon/PlayerWeaponController.h"
#include <memory>

class Camera;

class Player {
public:
    using ControlMode = PlayerMovementController::ControlMode;
    Player() = default;
    Player(const Player&) = delete;
    Player& operator=(const Player&) = delete;
    Player(Player&&) = delete;
    Player& operator=(Player&&) = delete;

    void Initialize(Model* model);
    void Update();
    void SetCamera(Camera* camera);
    Camera* GetCamera() const { return camera_; }
    void SetDebugCameraController(DebugCameraController* debugCameraController);
    void SetEnableLighting(bool isEnabled);

    const Vector3& GetTranslate() const { return transform_.translate; }
    const Vector3& GetRotate() const { return transform_.rotate; }
    const Vector3& GetScale() const { return transform_.scale; }
    void SetTranslate(const Vector3& translate);
    void SetRotate(const Vector3& rotate);
    void SetScale(const Vector3& scale) { transform_.scale = scale; }
    void SetRailFrame(const Vector3& railBasePosition, const Vector3& railRight,
        const Vector3& railUp, const Vector3& railForward);
    void ApplyRailAreaForce(const Vector3& force) { movementController_.ApplyRailAreaForce(force); }
    const Vector3& GetRailOffset() const { return movementController_.GetRailOffset(); }
    Vector3 GetAutomaticWorldVelocity() const;
    const Vector3& GetFlightForward() const { return movementController_.GetFlightForward(); }
    Vector3 GetEngineExhaustPosition() const;
    Vector3 GetEngineExhaustDirection() const;

    void EnableAllRangeMode(float areaRadius, float minHeight, float maxHeight);
    bool IsAllRangeMode() const { return movementController_.IsAllRangeMode(); }
    bool IsReturningToFlightArea() const { return movementController_.IsReturningToFlightArea(); }
    bool IsBoosting() const { return movementController_.IsBoosting(); }
    bool IsRolling() const { return movementController_.IsRolling(); }
    void SetControlMode(ControlMode mode) { movementController_.SetControlMode(mode); }
    ControlMode GetControlMode() const { return movementController_.GetControlMode(); }
    void SetMouseSensitivity(float sensitivity) { movementController_.SetMouseSensitivity(sensitivity); }
    float GetMouseSensitivity() const { return movementController_.GetMouseSensitivity(); }
    const Vector2& GetStarFoxSteeringInput() const { return movementController_.GetStarFoxSteeringInput(); }

    const Vector2& GetAimScreenPosition() const { return aimController_.GetAimScreenPosition(); }
    void FireBullet(const Camera& activeCamera) { weaponController_.FireBullet(activeCamera); }
    const std::vector<std::unique_ptr<PlayerBullet>>& GetBullets() const { return weaponController_.GetBullets(); }
    void SetHomingTargets(const std::vector<BaseEnemy*>& targets) { weaponController_.SetHomingTargets(targets); }
    void GetHomingLockPositions(std::vector<Vector3>& positions) const { weaponController_.GetHomingLockPositions(positions); }
    bool IsHomingMissileSelected() const { return weaponController_.IsHomingMissileSelected(); }
    const char* GetCurrentWeaponDisplayName() const { return weaponController_.GetCurrentWeaponName(); }
    float GetHeatRatio() const { return weaponController_.GetHeatRatio(); }
    bool IsOverheated() const { return weaponController_.IsOverheated(); }

    bool ApplyDamage(int damage);
    bool Heal(int amount) { return health_.Heal(amount); }
    bool IsDead() const { return health_.IsDead(); }
    bool IsDeathExplosionReady() const { return health_.IsDeathExplosionReady(); }
    int GetCurrentHp() const { return health_.GetCurrentHp(); }
    int GetMaxHp() const { return health_.GetMaxHp(); }
#if defined(ENABLE_DEVELOPMENT_TOOLS)
    void SetInvincibleMode(bool isEnabled) { health_.SetInvincibleMode(isEnabled); }
    bool IsInvincibleMode() const { return health_.IsInvincibleMode(); }
#endif

    void Draw();
    void DrawShadow(ShadowMapRenderer& renderer);
    void DrawImGui();

private:
    void ApplyTransform();
    void UpdateDeath();

    std::unique_ptr<Object3d> object_;
    Camera* camera_ = nullptr;
    DebugCameraController* debugCameraController_ = nullptr;
    EulerTransform transform_;
    PlayerAimController aimController_;
    PlayerMovementController movementController_{ transform_, aimController_ };
    PlayerHealth health_;
    PlayerWeaponController weaponController_{ transform_, aimController_, movementController_, health_ };
    bool isDebugMode_ = false;
};

#pragma once
#include "Engine/3D/Model.h"
#include "Engine/3D/Object3d.h"
#include "Engine/Winapp/WinApp.h"
#include "Engine/debugcamera/DebugCameraController.h"
#include "Engine/math/MathStruct.h"
#include <memory>
#include <vector>


#include "App/Game/Player/Bullet/PlayerBullet.h"

class Camera;
class Input;
class BaseEnemy;
struct Ray;

class Player {
public:
    enum class ControlMode {
        KeyboardAndMouse,
        StarFox,
    };

    void Initialize(Model* model);
    void Update();
    void Draw();

    void SetCamera(Camera* camera);
    void SetDebugCameraController(DebugCameraController* debugCameraController);
    void SetEnableLighting(bool enable);

    const Vector3& GetTranslate() const
    {
        return transform_.translate;
    }

    const Vector3& GetRotate() const
    {
        return transform_.rotate;
    }

    const Vector3& GetScale() const
    {
        return transform_.scale;
    }

    const Vector2& GetAimScreenPosition() const
    {
        return aimScreenPosition_;
    }

    const Vector3& GetRailOffset() const
    {
        return railOffset_;
    }

    Vector3 GetAutomaticWorldVelocity() const
    {
        if (IsDead()) {
            return { 0.0f, 0.0f, 0.0f };
        }
        return railForward_ * normalMaxSpeed_;
    }

    void ApplyRailAreaForce(const Vector3& force)
    {
        railOffset_.x += force.x;
        railOffset_.y += force.y;
        railOffset_ = ClampRailOffsetToScreen(railOffset_);
    }

    void SetTranslate(const Vector3& translate)
    {
        transform_.translate = translate;
        if (object_ != nullptr) {
            ApplyTransform();
            object_->Update();
        }
    }

    void SetRotate(const Vector3& rotate)
    {
        transform_.rotate.x = rotate.x;
        transform_.rotate.y = rotate.y;
        if (!isRolling_) {
            transform_.rotate.z = rotate.z;
        }
    }

    void SetScale(const Vector3& scale)
    {
        transform_.scale = scale;
    }

    void SetRailFrame(
        const Vector3& railBasePosition,
        const Vector3& railRight,
        const Vector3& railUp,
        const Vector3& railForward)
    {
        railBasePosition_ = railBasePosition;
        railRight_ = railRight;
        railUp_ = railUp;
        railForward_ = railForward;
    }

    const std::vector<std::unique_ptr<PlayerBullet>>& GetBullets() const
    {
        return bullets_;
    }

    bool IsBoosting() const
    {
        return isBoosting_;
    }

    bool IsRolling() const
    {
        return isRolling_;
    }

    bool ApplyDamage(int damage);
    bool Heal(int amount);
    bool IsDead() const
    {
        return currentHp_ <= 0;
    }
    bool IsDeathExplosionReady() const
    {
        return deathState_ == DeathState::Exploded;
    }
    int GetCurrentHp() const
    {
        return currentHp_;
    }
    int GetMaxHp() const
    {
        return maxHp_;
    }

    void DrawImGui();
    void FireBullet(const Camera& activeCamera);
    void SetHomingTargets(const std::vector<BaseEnemy*>& targets);
    void GetHomingLockPositions(std::vector<Vector3>& positions) const;
    bool IsHomingMissileSelected() const { return currentWeapon_ == kWeaponHomingMissile; }
    const char* GetCurrentWeaponDisplayName() const { return GetCurrentWeaponName(); }
    Camera* GetCamera() const { return camera_; }
    void SetControlMode(ControlMode mode)
    {
        if (controlMode_ != mode) {
            starFoxSteeringInput_ = { 0.0f, 0.0f };
        }
        controlMode_ = mode;
    }
    ControlMode GetControlMode() const { return controlMode_; }
    void SetMouseSensitivity(float sensitivity);
    float GetMouseSensitivity() const { return mouseSensitivity_; }
    const Vector2& GetStarFoxSteeringInput() const { return starFoxSteeringInput_; }


private:
    enum class DeathState {
        Alive,
        Falling,
        Exploded,
    };

    enum WeaponType {
        kWeaponNormalBullet = 0,
        kWeaponMissileBullet,
        kWeaponHomingMissile,
        kWeaponMinigun, // 新武器: 超高速連射ミニガン
        kWeaponCount
    };

public:
    float GetHeatRatio() const { return minigunHeat_; }

private:

    std::unique_ptr<Object3d> object_;
    std::vector<std::unique_ptr<PlayerBullet>> bullets_;
    std::shared_ptr<std::vector<BaseEnemy*>> homingTargets_ = std::make_shared<std::vector<BaseEnemy*>>();
    std::vector<BaseEnemy*> lockedHomingTargets_;
    bool wasHomingFireHeld_ = false;

    Model* bulletModel_ = nullptr;
    Camera* camera_ = nullptr;
    DebugCameraController* debugCameraController_ = nullptr;

    EulerTransform transform_;

    bool isDebugMode = false;
    bool isBoosting_ = false;
    ControlMode controlMode_ = ControlMode::KeyboardAndMouse;
    float mouseSensitivity_ = 1.0f;
    Vector2 starFoxSteeringInput_ = { 0.0f, 0.0f };

    // ローリング（バレルロール）用
    bool isRolling_ = false;
    int rollTimer_ = 0;
    int rollCooldown_ = 0;
    float rollDirection_ = 0.0f;
    int leftKeyTapTimer_ = 0;
    int rightKeyTapTimer_ = 0;
    static constexpr int kMaxTapInterval = 15;
    static constexpr int kRollDuration = 30;
    static constexpr int kRollCooldownDuration = 60;
    static constexpr float kRollSpeed = 0.8f;

    int maxHp_ = 20;
    int currentHp_ = maxHp_;
    int invincibleTimer_ = 0;
    static constexpr int kInvincibleFrames = 60;
    DeathState deathState_ = DeathState::Alive;
    float deathTimer_ = 0.0f;
    float deathFallVelocity_ = 0.0f;
    static constexpr float kDeathFallDuration = 1.2f;

    Vector2 aimScreenPosition_ = { 0.0f, 0.0f };

    float normalMaxSpeed_ = 0.5f;
    float boostMaxSpeed_ = 1.0f;
    float normalAcceleration_ = 0.2f;
    float boostAcceleration_ = 0.35f;
    float moveSpeed_ = normalAcceleration_;
    Vector3 velocity_ = { 0.0f, 0.0f, 0.5f };
    Vector3 railBasePosition_ = { 0.0f, 0.0f, 0.0f };
    Vector3 railRight_ = { 1.0f, 0.0f, 0.0f };
    Vector3 railUp_ = { 0.0f, 1.0f, 0.0f };
    Vector3 railForward_ = { 0.0f, 0.0f, 1.0f };
    Vector3 railOffset_ = { 0.0f, 0.0f, 0.0f };
    float railMoveLimitX_ = 20.0f;
    float railMoveLimitY_ = 12.0f;
    float playerClampMarginX_ = 100.0f;
    float playerClampMarginY_ = 100.0f;
    float playerBoundsHalfWidth_ = 1.5f;
    float playerBoundsHalfHeight_ = 1.0f;

    float bulletSpawnOffsetY_ = 0.3f;
    float bulletSpawnOffsetZ_ = 4.0f;
    float bulletSpeed_ = 5.8f; // 爆速化！照準を動かしても一瞬で即座に着弾！
    int currentWeapon_ = kWeaponNormalBullet;
    static constexpr int kNormalBulletDamage = 3;
    static constexpr int kNormalFireIntervalFrames = 10;
    int normalFireCooldown_ = 0;
    static constexpr int kMissileFireIntervalFrames = 120;
    static constexpr float kHomingLockMaxForwardDistance = 250.0f;
    static constexpr size_t kMaxHomingLockCount = 6;
    int missileFireCooldownFrames_ = kMissileFireIntervalFrames;

    // ミニガン（超高速連射＆熱気蓄積）用
    static constexpr int kMinigunDamage = 1;
    static constexpr int kMinigunFireIntervalFrames = 3;
    float minigunHeat_ = 0.0f;
    int minigunFireCooldown_ = 0;

    Vector3 CalculateMuzzlePosition() const;
    void CreateAimRay(Ray& aimRay, const Camera& activeCamera) const;
    Vector3 CreateConvergencePoint(const Ray& aimRay) const;
    Vector3 ResolveAimPoint(
        const Ray& aimRay,
        const Vector3& muzzlePosition) const;
    std::unique_ptr<PlayerBullet> CreateBullet(float& shotSpeed);
    void FireSingleBullet(const Camera& activeCamera, BaseEnemy* homingTarget);
    void UpdateHomingTarget(bool isLocking);
    void UpdateWeaponSwitch(Input* input);
    const char* GetCurrentWeaponName() const;
    void UpdateBullets();
    void RemoveDeadBullets();
    void ApplyTransform();
    void UpdateDeathAnimation();

    void UpdateKeyboardMove(Input* input);
    void UpdateStarFoxMove();
    void UpdateRolling(Input* input);
    void UpdateMouseAim();
    void ClampAimScreenPosition();
    Vector3 CalculateRailWorldPosition(const Vector3& railOffset) const;
    Vector2 CalculateScreenCorrection(const Vector3& railOffset) const;
    Vector3 ClampRailOffsetToScreen(const Vector3& railOffset) const;
    void UpdateScreenBounds(const Vector3& worldPosition, float& minX, float& maxX, float& minY, float& maxY) const;

#ifdef _DEBUG
private:
    bool drawDebugLines_ = false;
    Vector3 debugAimRayOrigin_ = { 0.0f, 0.0f, 0.0f };
    Vector3 debugAimPoint_ = { 0.0f, 0.0f, 0.0f };
    Vector3 debugMuzzlePosition_ = { 0.0f, 0.0f, 0.0f };
    Vector3 debugDrawRayOrigin_ = { 0.0f, 0.0f, 0.0f };
    Vector3 debugDrawAimPoint_ = { 0.0f, 0.0f, 0.0f };
#endif
};

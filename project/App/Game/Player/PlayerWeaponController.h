#pragma once
#include "Engine/math/MathStruct.h"
#include <array>
#include <cstdint>
#include <memory>
#include <vector>

class PlayerBullet;
class DxrRenderer;

class CombatTarget;

// PlayerWeaponController
// 選択中の武器を切り替え、発射・発熱・ロックオンと生成済みのプレイヤー弾を管理する。

class Model;
class Camera;
class Input;
class Player;

class PlayerWeaponController {
public:
    static constexpr float kMissileFireIntervalSeconds = 2.0f;
    static constexpr float kHomingFireIntervalSeconds = 4.0f;
    // 機体の姿勢と状態を参照する。参照先のPlayerは、この管理クラスより長く生存する必要がある。
    explicit PlayerWeaponController(const Player& player);
    // 前回の戦闘状態をリセットし、弾モデルを読み込む。設定済みのカメラは維持する。
    void Initialize();
    // 新しいカメラを記録し、すでに発射済みの弾にも同じカメラを設定する。
    void SetCamera(Camera* camera);
    // 武器切替・冷却・ミサイル間隔を更新し、選択中の武器へ射撃入力を渡す。
    void UpdateInput(Input* input, bool isDebugMode);
    // ボタンをすべて離すまで射撃とロックを受け付けない。
    void BlockFireUntilRelease();
    // 現在の武器へ発射を要求し、その武器固有の過熱やクールダウン判定を使う。
    bool FireBullet(const Camera& activeCamera);
    // 所有しているプレイヤー弾を一フレームずつ更新する。
    void UpdateBullets();
    // 命中や寿命で消滅した弾を一覧から外し、所有していたオブジェクトを破棄する。
    void RemoveDeadBullets();
    void DrawBullets();
    void SubmitRaytracing(DxrRenderer& renderer);
    const std::vector<std::unique_ptr<PlayerBullet>>& GetBullets() const { return bullets_; }
    uint64_t GetShotsFired() const { return shotsFired_; }
    void SetHomingTargets(const std::vector<CombatTarget*>& targets);
    void ClearHomingTargets();
    // 追尾ミサイル選択中だけ、ロック表示に必要な敵の位置を書き出す。
    void GetHomingLockPositions(std::vector<Vector3>& positions) const;
    void ResetHomingLock();
    bool IsHomingMissileSelected() const { return currentWeapon_ == kWeaponHomingMissile; }
    bool IsMissileSelected() const { return currentWeapon_ == kWeaponMissileBullet; }
    float GetMissileReloadRatio() const;
    const char* GetCurrentWeaponName() const;
    float GetHeatRatio() const { return heatRatio_; }
    bool IsOverheated() const { return isOverheated_; }
#ifdef _DEBUG
    // 照準レイと銃口からの射線を線で表示し、狙いのずれを確認できるようにする。
    void DrawAimDebugLines();
#endif

private:
    friend bool RunPlayerWeaponResetTests();
    friend bool RunPlayerWeaponPolymorphismTests();
    friend bool RunHomingInvalidationTests();
    friend struct PlayerWeaponHeatTestAccess;
    void ResetCombatState();
    void UpdateHeat(float deltaTimeSeconds, bool isFireHeld);
    void AddShotHeat();
    int GetShotDamage(int baseDamage) const;
    bool UpdateHomingFireInput(bool isFireHeld, bool isEnabled, Camera* camera,
        const Vector3& position, const Vector3& forward, const Vector2& aimPosition);
    void UpdateHomingTargets(bool isLocking, Camera* camera, const Vector3& position,
        const Vector3& forward, const Vector2& aimPosition);
    void GetLockedPositions(std::vector<Vector3>& positions) const;
    bool UpdateFireInputState(bool isFireHeld, bool isForeground, uint64_t focusLossCount);
    enum WeaponType {
        kWeaponAssault = 0,
        kWeaponMissileBullet,
        kWeaponHomingMissile,
        kWeaponCount
    };
    // ホイール・方向キー・数字キーの入力を武器の選択番号へ反映する。
    void UpdateWeaponSwitch(Input* input);
    void UpdateCooldowns(float deltaTimeSeconds);
    bool IsWeaponReady() const;
    bool TryStartCooldown();
    int GetRequestedShotCount(bool isHeld, bool isTriggered, float deltaTimeSeconds);
    // 機体に対する銃口の相対位置を、現在の姿勢でワールド位置へ変換する。
    Vector3 CalculateMuzzlePosition() const;
    // 武器に弾を生成させ、熱量の色・銃口位置・照準方向・機体速度を反映して所有する。
    void FireSingleBullet(const Camera& activeCamera, CombatTarget* homingTarget);

    const EulerTransform& transform_;
    const Player& player_;
    Model* bulletModel_ = nullptr;
    Camera* camera_ = nullptr;
    std::vector<std::unique_ptr<PlayerBullet>> bullets_;
    uint64_t shotsFired_ = 0;
    uint64_t lastFocusLossCount_ = 0;
    bool isFireBlocked_ = false;
    bool wasFireHeld_ = false;
    std::shared_ptr<std::vector<CombatTarget*>> homingTargets_ = std::make_shared<std::vector<CombatTarget*>>();
    std::vector<CombatTarget*> lockedTargets_;
    bool wasHomingFireHeld_ = false;
    float heatRatio_ = 0.0f;
    bool isOverheated_ = false;
    float graceRemainingSeconds_ = 0.0f;
    static constexpr float kGraceDurationSeconds = 30.0f / 60.0f;
    static constexpr float kAssaultShotHeat = 0.020f;
    static constexpr float kCoolingPerSecond = 0.45f;
    static constexpr float kOverheatCoolingPerSecond = 0.35f;
    static constexpr float kMaxForwardDistance = 250.0f;
    static constexpr size_t kMaxLockCount = 6;
    float bulletSpawnOffsetY_ = 0.3f;
    float bulletSpawnOffsetZ_ = 4.0f;
    float bulletSpeed_ = 5.8f;
    int currentWeapon_ = kWeaponAssault;
    static constexpr float kAssaultFireIntervalSeconds = 3.0f / 60.0f;
    static constexpr int kAssaultBaseDamage = 1;
    float assaultCooldownSeconds_ = 0.0f;
    double missileCooldownRemainingSeconds_ = 0.0;
    double homingCooldownRemainingSeconds_ = 0.0;
#ifdef _DEBUG
    bool shouldDrawDebugLines_ = false;
    Vector3 debugAimRayOrigin_ = {};
    Vector3 debugAimPoint_ = {};
    Vector3 debugMuzzlePosition_ = {};
    Vector3 debugDrawRayOrigin_ = {};
    Vector3 debugDrawAimPoint_ = {};
#endif
};


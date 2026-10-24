// 選択中の武器を切り替え、発射・発熱・ロックオンと生成済みのプレイヤー弾を管理する。
#pragma once
#include "App/Game/Player/Combat/Bullet/Common/PlayerBullet.h"
#include "App/Game/Player/Combat/Weapon/Common/BasePlayerWeapon.h"
#include "App/Game/Player/Combat/Weapon/Types/NormalPlayerWeapon.h"
#include "App/Game/Player/Combat/Weapon/Types/MinigunPlayerWeapon.h"
#include "App/Game/Player/Combat/Weapon/Types/MissilePlayerWeapon.h"
#include "App/Game/Player/Combat/Weapon/Types/HomingPlayerWeapon.h"
#include <array>
#include "Engine/math/MathStruct.h"
#include <memory>
#include <cstdint>
#include <vector>

class Model;
class Camera;
class Input;
class Player;

// 武器管理の補助状態。発熱とロックオンもこのファイルにまとめ、武器全体の状態を追いやすくする。
// 通常弾とミニガンで共有する熱量・冷却・過熱状態を保持する。
class PlayerWeaponHeat {
public:
    // 連射中の過熱猶予と、射撃停止中・過熱中の冷却を進める。
    void Update(float deltaTimeSeconds, bool isFireHeld);
    // 発射した武器に応じた熱量を加え、満タンになったときの猶予を準備する。
    void AddShotHeat(bool isMinigun);
    float GetHeatRatio() const { return heatRatio_; }
    bool IsOverheated() const { return isOverheated_; }
    // 熱量が高いほど威力を増やす。発射時点の熱量でその弾の威力を決める。
    int GetShotDamage(int baseDamage) const;

private:
    float heatRatio_ = 0.0f;
    bool isOverheated_ = false;
    float graceRemainingSeconds_ = 0.0f;
    static constexpr float kGraceDurationSeconds = 30.0f / 60.0f;
    static constexpr float kNormalShotHeat = 0.08f;
    static constexpr float kMinigunShotHeat = 0.025f;
    static constexpr float kCoolingPerSecond = 0.20f;
    static constexpr float kOverheatCoolingPerSecond = 0.25f;
};

// ロック候補と確定した対象を保持し、ボタンを離した瞬間の発射を判定する。
class PlayerHomingLock {
public:
    // 現在の候補一覧を共有データへ反映し、弾が対象の生存を確認できるようにする。
    void SetTargets(const std::vector<BaseEnemy*>& targets);
    // 発射済みミサイルも参照する対象一覧と、入力中のロックを無効化する。
    void ClearHomingTargets();
    // 押下中のロック更新と、ボタンを離した瞬間の発射要求を判定する。
    bool UpdateFireInput(bool isFireHeld, bool isEnabled, Camera* camera,
        const Vector3& position, const Vector3& forward, const Vector2& aimPosition);
    void ClearTargets() { lockedTargets_.clear(); }
    // ロック対象と前回の押下状態を消し、次の射撃入力を新しく受け付ける。
    void Reset();
    // 生存中のロック対象の位置を集め、画面のロックマーカー表示に使う。
    void GetPositions(std::vector<Vector3>& positions) const;
    const std::vector<BaseEnemy*>& GetLockedTargets() const { return lockedTargets_; }
    std::shared_ptr<const std::vector<BaseEnemy*>> GetTargets() const { return targets_; }

private:
    // 前方距離と画面上の照準との近さを調べ、条件を満たす敵を上限数までロックする。
    void UpdateTargets(bool isLocking, Camera* camera, const Vector3& position,
        const Vector3& forward, const Vector2& aimPosition);
    std::shared_ptr<std::vector<BaseEnemy*>> targets_ = std::make_shared<std::vector<BaseEnemy*>>();
    std::vector<BaseEnemy*> lockedTargets_;
    bool wasFireHeld_ = false;
    static constexpr float kMaxForwardDistance = 250.0f;
    static constexpr size_t kMaxLockCount = 6;
};

class PlayerWeaponController {
public:
    // 機体の姿勢と状態を参照する。参照先のPlayerは、この管理クラスより長く生存する必要がある。
    explicit PlayerWeaponController(const Player& player);
    // 前回の戦闘状態をリセットし、弾モデルを読み込む。設定済みのカメラは維持する。
    void Initialize();
    // 新しいカメラを記録し、すでに発射済みの弾にも同じカメラを設定する。
    void SetCamera(Camera* camera);
    // 武器切替・冷却・ミサイル間隔を更新し、選択中の武器へ射撃入力を渡す。
    void UpdateInput(Input* input, bool isDebugMode);
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
    void SetHomingTargets(const std::vector<BaseEnemy*>& targets) { homingLock_.SetTargets(targets); }
    void ClearHomingTargets() { homingLock_.ClearHomingTargets(); }
    // 追尾ミサイル選択中だけ、ロック表示に必要な敵の位置を書き出す。
    void GetHomingLockPositions(std::vector<Vector3>& positions) const;
    void ResetHomingLock() { homingLock_.Reset(); }
    bool IsHomingMissileSelected() const { return GetCurrentWeapon().IsHoming(); }
    const char* GetCurrentWeaponName() const;
    float GetHeatRatio() const { return weaponHeat_.GetHeatRatio(); }
    bool IsOverheated() const { return weaponHeat_.IsOverheated(); }
#ifdef _DEBUG
    // 照準レイと銃口からの射線を線で表示し、狙いのずれを確認できるようにする。
    void DrawAimDebugLines();
#endif

private:
    friend bool RunPlayerWeaponResetTests();
    void ResetCombatState();
    enum WeaponType {
        kWeaponNormalBullet = 0,
        kWeaponMissileBullet,
        kWeaponHomingMissile,
        kWeaponMinigun,
        kWeaponCount
    };
    // ホイール・方向キー・数字キーの入力を武器の選択番号へ反映する。
    void UpdateWeaponSwitch(Input* input);
    BasePlayerWeapon& GetCurrentWeapon() const { return *weapons_[currentWeapon_]; }
    // 発熱・ロック・機体情報と発射用の呼び出し先を、武器が使う情報へまとめる。
    PlayerWeaponContext MakeWeaponContext(const Camera* activeCamera = nullptr);
    // 機体に対する銃口の相対位置を、現在の姿勢でワールド位置へ変換する。
    Vector3 CalculateMuzzlePosition() const;
    // 有効なカメラがある場合だけ、現在の武器へ発射を要求する。
    bool FireCurrentCameraBullet();
    // 発射用カメラを選び、指定したロック対象への弾を生成する。
    void EmitBulletForCamera(const Camera* activeCamera, BaseEnemy* target);
    // 武器に弾を生成させ、熱量の色・銃口位置・照準方向・機体速度を反映して所有する。
    void FireSingleBullet(const Camera& activeCamera, BaseEnemy* homingTarget);

    const EulerTransform& transform_;
    const Player& player_;
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

#pragma once
#include "Engine/math/MathStruct.h"
#include "App/Game/Common/Bullet/BaseBullet.h"
#include "Engine/Effect/EffectManager.h"
#include <array>
#include <cstdint>
#include <memory>
#include <vector>

class DxrRenderer;

class CombatTarget;

// PlayerBullet
// プレイヤー弾に共通する敵への命中処理を定義し、敵弾とは別の型として扱う。

struct PlayerBulletExplosionSettings {
    float radius = 0.0f;
    float damage = 0.0f;
};

class PlayerBullet : public BaseBullet {
public:
    virtual ~PlayerBullet() = default;
    // 衝突時の実ダメージ。整数の基本威力に対して小数倍率を適用できる。
    virtual float GetHitDamage() const { return static_cast<float>(GetDamage()); }
    // 弾種固有の命中演出を追加するための共通の呼び出し口。標準では何も行わない。
    virtual void OnHitEnemy(const Vector3& position);
    // 半径が0なら爆発しない。衝突したフレームにだけ爆風を判定する。
    virtual PlayerBulletExplosionSettings GetExplosionSettings() const { return {}; }
};

// NormalBullet
// プレイヤーの通常弾の描画設定と、敵への命中時のエフェクトを扱う。
class NormalBullet : public PlayerBullet {
public:
    // 両翼から交互に撃つアサルトの威力を従来の半分にする。
    float GetHitDamage() const override { return PlayerBullet::GetHitDamage() * 0.5f; }
    // 通常弾の色・シェーダー・霧や影の設定を適用し、機体から見やすい弾にする。
    void Initialize(Model* model) override;
    void AlignToFlight(const Vector3& velocity);

    // 共通の移動と寿命処理を使い、通常弾を進める。
    void Update() override;

    // 通常弾の命中位置へヒットエフェクトを出す。
    void OnHitEnemy(const Vector3& position) override;
};

// MissileBullet
// ミサイルの描画・威力と航跡エフェクトを扱う。消滅時には航跡も停止する。



class MissileBullet : public PlayerBullet {
public:
    // ミサイルの見た目と威力を設定し、追従する航跡エフェクトを開始する。
    void Initialize(Model* model) override;
    // 移動と寿命を更新し、航跡を追従させる。消滅した場合は航跡を停止する。
    void Update() override;
    // 本体の位置を設定し、航跡の追従先も同じ位置へ更新する。
    void SetTranslate(const Vector3& translate) override;
    // 弾を消滅扱いにし、その場に航跡だけが残り続けないよう停止する。
    void SetDead() override;
   // void Move() override;
    float GetSpeed() const
    {
        return speed_;
    }
    // 命中位置にミサイル用の演出を出す。
    void OnHitEnemy(const Vector3& position) override;
    PlayerBulletExplosionSettings GetExplosionSettings() const override;
    // 球状エフェクトのEndScaleを、描画と共通のワールド半径として読む。
    static PlayerBulletExplosionSettings GetExplosionSettingsFromConfig(const nlohmann::json& config);

private:
    // ミサイルの位置を追いかけるループエフェクトを生成する。
    void CreateTrailEffect();
    // 弾の位置に航跡の発生位置を追従させる。
    void UpdateTrailPosition();
    // 再生中の航跡を停止し、エフェクトのハンドルを無効化する。
    void StopTrailEffect();

private:
    static constexpr float kExplosionDamage = 5.0f;
    float speed_ = 180.0f;
    float scale_ = 2.0f;

    EffectHandle trailEffectHandle_ = kInvalidEffectHandle;

    std::shared_ptr<Vector3> trailPosition_;
};

// HomingMissileBullet
// 生存しているロック対象へ進行方向を曲げ、目標を通り過ぎた回数も管理するミサイル。


class CombatTarget;

class HomingMissileBullet : public MissileBullet {
public:
    // 追尾弾は直撃だけ。親のミサイル用爆発演出と範囲ダメージを使わない。
    void OnHitEnemy(const Vector3& position) override;
    PlayerBulletExplosionSettings GetExplosionSettings() const override { return {}; }
    // ロック対象と候補一覧を記録する。候補一覧は弱参照で保持し、寿命を引き延ばさない。
    void SetTarget(CombatTarget* target, const std::shared_ptr<const std::vector<CombatTarget*>>& activeTargets);

protected:
    // 対象の生存を確認して進行方向を曲げ、接近時の補正と通過回数によって追尾を制御する。
    void Move() override;

private:
    CombatTarget* target_ = nullptr;
    std::weak_ptr<const std::vector<CombatTarget*>> activeTargets_;
    float homingStrength_ = 0.08f;
    int targetPassCount_ = 0;

    static constexpr float kCloseHomingDistance = 50.0f;
    static constexpr float kCloseHomingStrength = 0.32f;
    static constexpr float kTargetSnapDistance = 10.0f;
    static constexpr int kMaxTargetPassCount = 2;
};

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
    float GetHeatRatio() const { return heatGauge_; }
    // 照準表示用の基準位置。交互発射でも照準が左右に揺れないよう中央を使う。
    Vector3 CalculateMuzzlePosition() const;
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
    struct FireInput {
        bool isHeld;       // 有効な長押し
        bool isTriggered;  // 有効な押した瞬間
    };
    FireInput ReadFireInput(Input* input);
    void ResetCombatState();
    void UpdateAssaultInput(float deltaTimeSeconds, bool isFireHeld, bool isTriggered);
    void UpdateMissileInput(bool isTriggered);
    void UpdateHomingInput(bool isFireHeld);
    void UpdateAssaultHeat(float deltaTimeSeconds, bool isAssaultFireActive);
    void AddShotHeat();
    int GetShotDamage(int baseDamage) const;
    bool UpdateHomingFireInput(bool isFireHeld, bool isEnabled, Camera* camera,
        const Vector3& position, const Vector3& forward, const Vector2& aimPosition);
    void UpdateHomingTargets(bool isLocking, Camera* camera, const Vector3& position,
        const Vector3& forward, const Vector2& aimPosition);
    void GetLockedPositions(std::vector<Vector3>& positions) const;
    // 生存・弾モデル・カメラ・デバッグ状態から射撃の基本条件を確認する。
    bool CanFire(bool isDebugMode) const;
    // 入力機器のいずれかで射撃ボタンが押されているかを確認する。
    bool IsPhysicalFireHeld(Input* input) const;
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
    void StartCooldown();
    int GetRequestedShotCount(bool isHeld, bool isTriggered, float deltaTimeSeconds);
    // 武器に弾を生成させ、熱量の色・銃口位置・照準方向・機体速度を反映して所有する。
    void FireSingleBullet(const Camera& activeCamera, CombatTarget* homingTarget);
    Vector3 CalculateShotMuzzlePosition() const;

    const EulerTransform& transform_;
    const Player& player_;
    Model* bulletModel_ = nullptr;
    Camera* camera_ = nullptr;
    std::vector<std::unique_ptr<PlayerBullet>> bullets_;
    uint64_t shotsFired_ = 0;
    bool isFireBlocked_ = false;
    std::shared_ptr<std::vector<CombatTarget*>> homingTargets_ = std::make_shared<std::vector<CombatTarget*>>();
    std::vector<CombatTarget*> lockedTargets_;
    bool wasHomingFireHeld_ = false;
    float heatGauge_ = 0.0f; // 熱量ゲージ（0〜1）
    bool isOverheated_ = false;
    //熱量が満タンになってから、オーバーヒートするまでの残り猶予時間
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
    bool isNextAssaultShotLeft_ = true;
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


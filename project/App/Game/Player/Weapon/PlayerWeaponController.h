// 選択中の武器を切り替え、発射・発熱・ロックオンと生成済みのプレイヤー弾を管理する。
#pragma once
#include "App/Game/Player/Bullet/Common/PlayerBullet.h"
#include "App/Game/Player/Weapon/Common/PlayerWeaponHeat.h"
#include "App/Game/Player/Weapon/Common/PlayerHomingLock.h"
#include "App/Game/Player/Weapon/Common/BasePlayerWeapon.h"
#include "App/Game/Player/Weapon/Types/NormalPlayerWeapon.h"
#include "App/Game/Player/Weapon/Types/MinigunPlayerWeapon.h"
#include "App/Game/Player/Weapon/Types/MissilePlayerWeapon.h"
#include "App/Game/Player/Weapon/Types/HomingPlayerWeapon.h"
#include <array>
#include "Engine/math/MathStruct.h"
#include <memory>
#include <cstdint>
#include <vector>

class Model;
class Camera;
class Input;
class Player;

class PlayerWeaponController {
public:
    // 機体の姿勢と状態を参照する。参照先のPlayerは、この管理クラスより長く生存する必要がある。
    explicit PlayerWeaponController(const Player& player);
    // 発射回数をリセットし、プレイヤー弾の表示に使うモデルを読み込む。
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

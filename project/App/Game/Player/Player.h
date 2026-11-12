#pragma once
#include "App/Game/Player/Aim/PlayerReticleController.h"
#include "App/Game/Player/Aim/PlayerShotPointCalculator.h"
#include "App/Game/Common/Bullet/BaseBullet.h"
#include "Engine/Effect/EffectManager.h"
#include <memory>
#include <vector>
#include <array>
#include "Engine/math/MathStruct.h"
#include <cstdint>
#include "App/Game/Stage/RailHeightRange.h"
#include "Engine/3D/Model.h"
#include "Engine/3D/Object3d.h"
#include "Engine/debugcamera/DebugCameraController.h"

// PlayerBullet
// プレイヤー弾に共通する敵への命中処理を定義し、敵弾とは別の型として扱う。

struct PlayerBulletExplosionSettings {
    float radius = 0.0f;
    float damage = 0.0f;
};

class PlayerBullet : public BaseBullet {
public:
    virtual ~PlayerBullet() = default;
    // 弾種固有の命中演出を追加するための共通の呼び出し口。標準では何も行わない。
    virtual void OnHitEnemy(const Vector3& position);
    // 半径が0なら爆発しない。衝突したフレームにだけ爆風を判定する。
    virtual PlayerBulletExplosionSettings GetExplosionSettings() const { return {}; }
};

// NormalBullet
// プレイヤーの通常弾の描画設定と、敵への命中時のエフェクトを扱う。
class NormalBullet : public PlayerBullet {
public:
    // 通常弾の色・シェーダー・霧や影の設定を適用し、機体から見やすい弾にする。
    void Initialize(Model* model) override;

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

#include "App/Game/Player/PlayerWeaponController.h"

// PlayerMovement
class Camera;

// 操舵・飛行・レール上の自機位置を管理する。PlayerとStateには依存しない。
class PlayerMovement {
public:
    void SetCamera(const Camera* camera) { camera_ = camera; }
    const Vector3& GetRailOffset() const { return railOffset_; }
    const Vector3& GetRailBasePosition() const { return railBasePosition_; }
    const Vector3& GetFlightForward() const { return railForward_; }
    const Vector3& GetRailRight() const { return railRight_; }
    const Vector3& GetRailUp() const { return railUp_; }
    bool IsAllRangeMode() const { return isAllRangeMode_; }
    bool IsReturningToFlightArea() const { return isReturningToFlightArea_; }
    bool IsBoosting() const { return isBoosting_; }
    float GetForwardSpeedMultiplier() const { return velocity_.z / normalMaxSpeed_; }
    float GetMouseSensitivity() const { return mouseSensitivity_; }
    const Vector2& GetSteeringInput() const { return steeringInput_; }
    void SetSteeringInput(const Vector2& steering) { steeringInput_ = steering; }
    Vector3 GetWorldVelocity() const { return railForward_ * velocity_.z; }
    void SetRailMinimumHeight(float height) { railMinimumHeight_ = height; hasRailMinimumHeight_ = true; hasRailHeightRange_ = false; }
    void SetRailHeightRange(const RailHeightRange& range) { railHeightRange_ = range; hasRailHeightRange_ = true; }
    void ResetSteering() { steeringInput_ = {}; }
    void UpdateMovement(EulerTransform& transform, const Vector2& steering, float railResponse,
        float deltaTimeSeconds);
    void InitializeMovement(const EulerTransform& transform);
    void SynchronizePosition(const EulerTransform& transform);
    void SynchronizeOrientation(const EulerTransform& transform);
    void ApplyRailPosition(EulerTransform& transform);
    void StopMovement();
    Vector3 GetAutomaticMovementVelocity() const;
    void SetRailFrame(const Vector3& railBasePosition,
        const Vector3& railRight, const Vector3& railUp, const Vector3& railForward);
    void ApplyRailAreaForce(const Vector3& force, float deltaTimeSeconds);
    // ブーストを開始したフレームだけtrueを返す。効果音は呼び出し側で再生する。
    bool UpdateBoost(bool isBraking, bool shouldBoost);
    void EnableAllRangeMode(const EulerTransform& transform, float areaRadius, float minHeight, float maxHeight);
    void SetMouseSensitivity(float sensitivity);
    void UpdateSteering(const Vector2& aimPosition, const Vector2& screenSizePixels,
        bool isAllRangeMode, float deltaTimeSeconds, bool isGamepad);
    Vector3 CalculateRailWorldPosition(const Vector3& railOffset) const;
    Vector3 ClampRailOffsetToScreen(const Vector3& railOffset) const;

private:
    void UpdateAllRangeMove(EulerTransform& transform, Vector2 steering, float deltaTimeSeconds);
    void UpdateFlightOrientation(const Vector2& steering, float deltaTimeSeconds);
    void UpdateFlightAreaReturn(const EulerTransform& transform, Vector2& steering, float deltaTimeSeconds);
    void UpdateFlightHeight(const EulerTransform& transform, float deltaTimeSeconds);
    void UpdateFlightPosition(EulerTransform& transform, float deltaTimeSeconds);
    Vector2 CalculateScreenCorrection(const Vector3& railOffset) const;
    void UpdateScreenBounds(const Vector3& worldPosition, float& minX, float& maxX, float& minY, float& maxY) const;
    void EnforceHeightRange(Vector3& offset) const;
    // カメラは外部所有。Playerから設定された参照を利用する。
    const Camera* camera_ = nullptr;

    bool isBoosting_ = false;
    bool isAllRangeMode_ = false;
    bool isReturningToFlightArea_ = false;
    float flightAreaRadius_ = 600.0f;
    float flightMinHeight_ = 5.0f;
    float flightMaxHeight_ = 180.0f;
    float flightYaw_ = 0.0f;
    float flightPitch_ = 0.0f;
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

    float mouseSensitivity_ = 1.0f; // 中央からの照準のずれに掛ける倍率。
    Vector2 steeringInput_ = {}; // 補間済みの入力。Xは右向き、Yは上向きが正で、中央は0。

    float railMoveLimitX_ = 20.0f;
    float railMoveLimitY_ = 15.0f;
    float railMinimumHeight_ = 0.0f;
    bool hasRailMinimumHeight_ = false;
    RailHeightRange railHeightRange_;
    bool hasRailHeightRange_ = false;
    // 画面端に残す余白。単位はピクセル。
    float playerClampMarginX_ = 10.0f;
    float playerClampMarginY_ = 10.0f;
    // 補正判定に使う機体の半幅・半高さ。単位はワールド座標。
    float playerBoundsHalfWidth_ = 1.5f;
    float playerBoundsHalfHeight_ = 1.0f;

};

// PlayerInput
class Input;

// 入力機器の状態を、自機の操作に使う値へ変換する。PlayerとMovementは参照しない。
class PlayerInput {
public:
    enum class ControlInputType { Keyboard, Mouse, Gamepad };

    struct AimInput {
        Vector2 stick = {};
        bool isPitchInverted = false;
        float sensitivity = 1.0f;
    };

    struct BoostInput {
        bool isBraking = false;
        bool shouldBoost = false;
    };

    static ControlInputType GetControlInputType(const Input& input, bool shouldUseMouse);
    static Vector2 ReadMousePosition();
    static AimInput ReadGamepadAim(const Input& input);
    static Vector2 ReadKeyboardSteering(const Input& input);
    static BoostInput ReadBoost(const Input& input, bool isEnabled);
};

// PlayerEntryState
class Player;

// Player固有の登場演出はまだないため、待ち時間を加えず戦闘へ進む。
class PlayerEntryState {
public:
    void Update(Player& player);
};

// PlayerBattleState
class Player;

// 通常操作中の照準・移動・射撃の更新順序を管理する。
class PlayerBattleState {
public:
    void Update(Player& player);
};

// PlayerDeadState
class Player;

// 死亡開始時の操作解除と、機体の落下・回転を管理する。
class PlayerDeadState {
public:
    void Enter(Player& player);
    void Update(Player& player);
    void UpdateAnimation(EulerTransform& transform, float deltaTimeSeconds);
    bool IsExplosionReady() const { return isExplosionReady_; }
    // 落下中は機体を表示し、爆発へ進める状態になったら非表示にする。
    bool ShouldDraw() const { return !isExplosionReady_; }

private:
    float deathTimerSeconds_ = 0.0f;
    float deathFallVelocity_ = 0.0f;
    bool isExplosionReady_ = false;
    static constexpr float kDeathFallDurationSeconds = 1.2f;
};

// Player
// Playerの姿勢と公開APIを保持し、登場・戦闘・死亡の更新を各Stateへ委譲する。

class Camera;
class Input;
struct Ray;

// 自機の状態遷移・共有データ・描画を管理する。
class Player {
public:
    static constexpr float kDirectAimYawRadians = 0.45f;
    static constexpr float kDirectAimPitchRadians = 0.33f;
    bool IsUsingDirectGamepadControl() const { return isUsingDirectGamepadControl_; }
    const Vector2& GetDirectGamepadAimInput() const { return directGamepadAimInput_; }
    void UpdateAfterCamera(const Camera& activeCamera, float deltaTimeSeconds);
    enum class ControlMode { KeyboardAndMouse, Controller };
    Player() = default;
    // 武器管理がPlayerを参照するため、コピー・ムーブを禁止する。
    Player(const Player&) = delete;
    Player& operator=(const Player&) = delete;
    Player(Player&&) = delete;
    Player& operator=(Player&&) = delete;

    // 使用クラス：GamePlayScene。モデル・武器・移動・照準の初期状態を設定する。
    void Initialize(Model* model);
    // 使用クラス：GamePlayScene。照準・移動・発射・描画姿勢を順に更新する。
    void Update();
    // レール進行の計算前に、このフレームのブースト入力を確定する。
    void UpdateBoostState();
    // 使用クラス：GamePlayScene。機体・移動・武器で使うカメラを揃える。
    void SetCamera(Camera* camera);
    // 設定中のカメラを返す。カメラの所有権はPlayerにない。
    Camera* GetCamera() const { return camera_; }
    // 使用クラス：GamePlayScene。デバッグカメラ中に通常操作を止めるための参照を設定する。
    void SetDebugCameraController(DebugCameraController* debugCameraController);
    // 機体のライティングを切り替える。未初期化なら何もしない。
    void SetEnableLighting(bool isEnabled);

    // 機体のワールド位置を返す。シーン配置・敵の照準・衝突判定などで参照する。
    const Vector3& GetTranslate() const { return transform_.translate; }
    // 機体の回転を返す。
    const Vector3& GetRotate() const { return transform_.rotate; }
    // 機体の拡大率を返す。
    const Vector3& GetScale() const { return transform_.scale; }
    // 使用クラス：GamePlayScene。位置を変更し、移動側の基準位置と描画姿勢にも反映する。
    void SetTranslate(const Vector3& translate);
    // 使用クラス：GamePlayScene。ロール中のZ回転を保ち、全方向飛行の内部姿勢も同期する。
    void SetRotate(const Vector3& rotate);
    // 機体の拡大率を設定する。描画オブジェクトへの反映は次のApplyTransformで行う。
    void SetScale(const Vector3& scale) { transform_.scale = scale; }
    // 使用クラス：GamePlayScene。コース上の基準位置と左右・上下・前方向を移動側へ渡す。
    void SetRailFrame(const Vector3& railBasePosition, const Vector3& railRight,
        const Vector3& railUp, const Vector3& railForward);
    // ステージの外力をレール上の移動量へ反映する。
    void ApplyRailAreaForce(const Vector3& force);
    // 使用クラス：GamePlayScene。レール基準位置からの左右・上下の移動量を返す。
    const Vector3& GetRailOffset() const { return movement_.GetRailOffset(); }
    // 操作による移動量を含まない、現在のレール基準位置。
    const Vector3& GetRailBasePosition() const { return movement_.GetRailBasePosition(); }
    void SetRailMinimumHeight(float height) { movement_.SetRailMinimumHeight(height); }
    void SetRailHeightRange(const RailHeightRange& range) { movement_.SetRailHeightRange(range); }
    // 敵の照準などに使う前進速度を返す。死亡時は0、レール飛行では通常速度を使う。
    Vector3 GetAutomaticWorldVelocity() const;
    // 使用クラス：GamePlaySceneなど。カメラ配置やエフェクトに使う飛行方向を返す。
    const Vector3& GetFlightForward() const { return movement_.GetFlightForward(); }
    // 使用クラス：GamePlayScene。噴射エフェクトを置く機体後方のワールド位置を返す。
    Vector3 GetEngineExhaustPosition() const;
    // 使用クラス：GamePlayScene。機体の回転に合わせた噴射方向を返す。
    Vector3 GetEngineExhaustDirection() const;

    // 使用クラス：GamePlayScene。指定した飛行範囲で全方向飛行へ切り替える。
    void EnableAllRangeMode(float areaRadius, float minHeight, float maxHeight);
    // レール飛行と全方向飛行を区別するための状態を返す。
    bool IsAllRangeMode() const { return movement_.IsAllRangeMode(); }
    // 使用クラス：GamePlayScene。飛行範囲の内側への自動復帰中かを返す。
    bool IsReturningToFlightArea() const { return movement_.IsReturningToFlightArea(); }
    // 使用クラス：GamePlayScene。カメラ・噴射・ポストエフェクトの切り替えに使う。
    bool IsBoosting() const { return movement_.IsBoosting(); }
    float GetForwardSpeedMultiplier() const { return movement_.GetForwardSpeedMultiplier(); }
    void ResetHomingLock() { weaponController_.ResetHomingLock(); }
    void BlockFireUntilRelease() { weaponController_.BlockFireUntilRelease(); }
    // 使用クラス：GamePlayScene。キーボード移動とマウス操舵を切り替える。
    void SetControlMode(ControlMode mode);
    // 現在の操作方式を返す。
    ControlMode GetControlMode() const { return controlMode_; }
    // 使用クラス：GamePlayScene。マウス操舵の感度を移動側へ設定する。
    void SetMouseSensitivity(float sensitivity);
    // 現在のマウス操舵の感度を返す。
    float GetMouseSensitivity() const { return movement_.GetMouseSensitivity(); }
    // 使用クラス：GamePlayScene。照準へ機首を向けるための操舵入力を返す。
    const Vector2& GetStarFoxSteeringInput() const { return movement_.GetSteeringInput(); }

    // GamePlaySceneのレティクルとオーバーヒートゲージの配置に使う照準位置を返す。
    const Vector2& GetAimScreenPosition() const { return reticleController_.GetAimScreenPosition(); }
    Vector3 GetNearAimPoint(const Camera& activeCamera) const;
    // 操縦・射撃の前と、描画用カメラ更新後にパッド照準の距離を補正する。
    void ConstrainGamepadAimToPlayer();
    // 外部から現在の武器の発射を要求する。死亡・過熱・クールダウン中は発射されない。
    void FireBullet(const Camera& activeCamera) { weaponController_.FireBullet(activeCamera); }
    // 使用クラス：GameplayCollisionSystem。プレイヤー弾の衝突判定に使う一覧を返す。
    const std::vector<std::unique_ptr<PlayerBullet>>& GetBullets() const { return weaponController_.GetBullets(); }
    uint64_t GetShotsFired() const { return weaponController_.GetShotsFired(); }
    // 使用クラス：GamePlayScene。生存中の敵をロックオン候補として武器側へ渡す。
    void SetHomingTargets(const std::vector<CombatTarget*>& targets) { weaponController_.SetHomingTargets(targets); }
    // 敵の破棄前に、発射済みミサイルを含む対象への参照を無効化する。
    void ClearHomingTargets() { weaponController_.ClearHomingTargets(); }
    // 使用クラス：GamePlayScene。ロック表示用の座標を引数の一覧に書き込む。
    void GetHomingLockPositions(std::vector<Vector3>& positions) const { weaponController_.GetHomingLockPositions(positions); }
    // ホーミングミサイルが選択されているかを返す。
    bool IsHomingMissileSelected() const { return weaponController_.IsHomingMissileSelected(); }
    bool IsMissileSelected() const { return weaponController_.IsMissileSelected(); }
    float GetMissileReloadRatio() const { return weaponController_.GetMissileReloadRatio(); }
    // 使用クラス：GamePlayScene。武器HUDに表示する名前を返す。
    const char* GetCurrentWeaponDisplayName() const { return weaponController_.GetCurrentWeaponName(); }
    // 使用クラス：GamePlayScene。ゲージ用の熱量を0～1で返す。
    float GetHeatRatio() const { return weaponController_.GetHeatRatio(); }
    // 使用クラス：GamePlayScene。過熱表示の切り替えに使う状態を返す。
    bool IsOverheated() const { return weaponController_.IsOverheated(); }

    // 使用クラス：GameplayCollisionSystem・GamePlayScene。被弾を判定し、死亡時は操作状態を解除する。
    bool ApplyDamage(int damage);
    // 使用クラス：GamePlayScene。回復アイテムの回復量をHPへ反映する。
    bool Heal(int amount);
    // 使用クラス：GamePlaySceneなど。HPが0以下かを返す。死亡演出の完了とは別の判定。
    bool IsDead() const { return currentHp_ <= 0; }
    // 使用クラス：GamePlayScene。落下演出後の爆発・シーン遷移へ進めるかを返す。
    bool IsDeathExplosionReady() const { return state_ == State::Dead && deadState_.IsExplosionReady(); }
    // 使用クラス：GamePlayScene。HPのHUD表示と被弾検出に使う現在値を返す。
    int GetCurrentHp() const { return currentHp_; }
    // 使用クラス：GamePlayScene。HP表示の上限を返す。
    int GetMaxHp() const { return maxHp_; }
#if defined(ENABLE_DEVELOPMENT_TOOLS)
    // 開発用の無敵モードを設定する。
    void SetInvincibleMode(bool isEnabled) { isInvincibleMode_ = isEnabled; }
    // 開発用の無敵モードが有効かを返す。
    bool IsInvincibleMode() const { return isInvincibleMode_; }
#endif

    // 使用クラス：GamePlayScene。弾を描画し、死亡状態や被弾中の点滅に応じて機体を描画する。
    void Draw();
    void SubmitRaytracing(DxrRenderer& renderer);
    // 機体の影を描画する。機体の非表示中は影も描かない。
    void DrawShadow(ShadowMapRenderer& renderer);
    // デバッグビルドでHP・武器・熱量を表示する。
    void DrawImGui();
    bool IsAimDistanceSmoothingEnabled() const { return shotPointCalculator_.IsAimDistanceSmoothingEnabled(); }
    void SetAimDistanceSmoothingEnabled(bool isEnabled) { shotPointCalculator_.SetAimDistanceSmoothingEnabled(isEnabled); }

private:
    friend class PlayerWeaponController;
    friend struct PlayerTestAccess;
    friend class PlayerEntryState;
    friend class PlayerBattleState;
    friend class PlayerDeadState;

    enum class State { Entry, Battle, Dead };
    void ChangeState(State state);
    State state_ = State::Entry;
    PlayerEntryState entryState_;
    PlayerBattleState battleState_;
    PlayerDeadState deadState_;

    // 照準の計算は専用クラスに委譲する。
    void InitializeAim() {
        reticleController_.InitializeAim();
        shotPointCalculator_.Initialize();
    }
    void UpdateMouseAim() { reticleController_.UpdateMouseAim(); }
    void UpdateGamepadAim(float x, float y, bool inverted, float dt, float sensitivity = 1.0f) { reticleController_.UpdateGamepadAim(x, y, inverted, dt, sensitivity); }
    void ClampAimScreenPosition() { reticleController_.ClampAimScreenPosition(); }
    void ClampGamepadAimToPlayer(const Vector2& position) { reticleController_.ClampGamepadAimToPlayer(position); }
    Vector2 GetGamepadSteeringScreenPosition() const { return reticleController_.GetGamepadSteeringScreenPosition(); }
    void UpdateAim(const Camera& camera, float deltaTimeSeconds) {
        shotPointCalculator_.Update(camera, GetAimScreenPosition(), deltaTimeSeconds);
    }
    Vector3 GetAimPoint(const Camera& camera) const { return shotPointCalculator_.GetAimPoint(camera, GetAimScreenPosition()); }
    Vector3 GetAimRayOrigin(const Camera& camera) const { return shotPointCalculator_.GetAimRayOrigin(camera, GetAimScreenPosition()); }

    // 入力方式はパッドを優先し、その他は設定された操作方式を使う。
    using ControlInputType = PlayerInput::ControlInputType;
    struct ControlInput {
        Vector2 steering;
        float railResponse;
    };
    // 使用中のパッドを優先し、パッドでなければ設定された操作方式を選ぶ。
    ControlInputType GetControlInputType(const Input& input) const;
    // 選択された操作方式に合わせて、マウスまたはパッドの照準を更新する。
    void UpdateAimFromInput(const Input& input, float deltaTimeSeconds);
    // キーボード・マウス・パッドの違いを、移動処理が共通に扱える入力へまとめる。
    ControlInput GetControlInput(const Input& input, const Vector2& screenSizePixels, bool isAllRangeMode, float deltaTimeSeconds);
    // 照準の画面位置を機体の左右・上下の操舵入力へ変換する。
    ControlInput UpdateAimSteering(const Vector2& screenSizePixels, bool isAllRangeMode, float deltaTimeSeconds, bool isGamepad);

    // 移動・操舵・ロール・画面内補正。
    void InitializeMovement();
    // 外部から設定された機体位置を、レール基準の移動状態へ反映する。
    void SynchronizePosition();
    // 外部から設定された回転角を、全方向飛行の機首方向へ反映する。
    void SynchronizeOrientation();
    // 加速・減速の入力を確認し、目標速度へ滑らかに近づける。
    void UpdateBoost(Input* input, bool isDebugMode);
    // 操作入力を読み、レール飛行か全方向飛行かに応じて移動処理を選ぶ。
    void UpdateMovement(Input* input);
    // レール基準位置と操縦によるずれを合成し、画面内に収まる機体位置を決める。
    void ApplyRailPosition();
    // 死亡や操作停止時に、速度とロールなどの移動状態を解除する。
    void StopMovement();
    Vector3 GetAutomaticMovementVelocity() const;
    Vector3 GetWorldVelocity() const { return movement_.GetWorldVelocity(); }
    // 照準の中心からのずれにデッドゾーンと感度を反映し、操舵入力を滑らかにする。
    void UpdateSteering(const Vector2& aimPosition, const Vector2& screenSizePixels,
        bool isAllRangeMode, float deltaTimeSeconds, bool isGamepad = false);
    void ResetSteering() { movement_.ResetSteering(); }
    // 左右のロール開始と進行を処理し、一回転後に通常姿勢へ戻す。
    // 機体の大きさとカメラ投影を考慮し、画面からはみ出ないレール上のずれへ補正する。
    Vector3 ClampRailOffsetToScreen(const Vector3& railOffset) const;
    // レールの基準位置に左右・上下のずれを加え、ワールド位置へ変換する。
    Vector3 CalculateRailWorldPosition(const Vector3& railOffset) const;

    // HP・無敵時間。
    bool ApplyHealthDamage(int damage);
    // 被弾後の無敵時間を減らし、時間切れで通常の被弾状態へ戻す。
    void UpdateInvincibility(float deltaTimeSeconds);
    bool IsAlive() const { return state_ != State::Dead; }
    bool IsFalling() const { return state_ == State::Dead && !deadState_.IsExplosionReady(); }
    // 死亡状態と被弾中の点滅を調べ、今のフレームで機体を描画するかを返す。
    bool ShouldDraw() const;

    PlayerReticleController reticleController_;
    PlayerShotPointCalculator shotPointCalculator_;
    bool isUsingDirectGamepadControl_ = false;
    bool shouldUpdateWeaponsAfterCamera_ = false;
    Vector2 directGamepadAimInput_ {};
    void SynchronizeGamepadControlStyle(const Input& input);
    ControlInput GetDirectGamepadControl(const Input& input, float deltaTimeSeconds);
    Vector3 GetDirectGamepadAimDirection() const;

    int maxHp_ = 20;
    int currentHp_ = maxHp_;
    float invincibleRemainingSeconds_ = 0;
#if defined(ENABLE_DEVELOPMENT_TOOLS)
    bool isInvincibleMode_ = false;
#endif
    static constexpr float kInvincibleDurationSeconds = 1.0f;

    ControlMode controlMode_ = ControlMode::Controller;
    PlayerMovement movement_;

    // 使用クラス：本クラス。保持している位置・回転・拡大率を描画オブジェクトへ渡す。
    void ApplyTransform();

    std::unique_ptr<Object3d> object_;
    // カメラ類は外部所有。参照先はPlayerを使用している間、有効である必要がある。
    Camera* camera_ = nullptr;
    DebugCameraController* debugCameraController_ = nullptr;
    // 機体の位置・回転・大きさを保持し、移動処理と描画で共有する。
    EulerTransform transform_;
    // 発射と弾を管理し、BattleとDeadの両方から更新する。
    PlayerWeaponController weaponController_{ *this };
    bool isDebugMode_ = false;
};

// プレイヤーの照準・操作・移動・HP・死亡演出を扱う。武器と弾の管理はPlayerWeaponControllerへ任せる。
#pragma once
#include "Engine/3D/Model.h"
#include "Engine/3D/Object3d.h"
#include "Engine/debugcamera/DebugCameraController.h"
#include "Engine/math/MathStruct.h"
#include "App/Game/Player/Weapon/PlayerWeaponController.h"
#include <memory>

class Camera;
class Input;
struct Ray;

// 自機の照準・操作・移動・HPと描画を管理する。武器管理は専用クラスへ任せる。
class Player {
public:
    enum class ControlMode { KeyboardAndMouse, StarFox };
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
    const Vector3& GetRailOffset() const { return railOffset_; }
    void SetRailMinimumHeight(float height) { railMinimumHeight_ = height; hasRailMinimumHeight_ = true; }
    // 敵の照準などに使う前進速度を返す。死亡時は0、レール飛行では通常速度を使う。
    Vector3 GetAutomaticWorldVelocity() const;
    // 使用クラス：GamePlaySceneなど。カメラ配置やエフェクトに使う飛行方向を返す。
    const Vector3& GetFlightForward() const { return railForward_; }
    // 使用クラス：GamePlayScene。噴射エフェクトを置く機体後方のワールド位置を返す。
    Vector3 GetEngineExhaustPosition() const;
    // 使用クラス：GamePlayScene。機体の回転に合わせた噴射方向を返す。
    Vector3 GetEngineExhaustDirection() const;

    // 使用クラス：GamePlayScene。指定した飛行範囲で全方向飛行へ切り替える。
    void EnableAllRangeMode(float areaRadius, float minHeight, float maxHeight);
    // レール飛行と全方向飛行を区別するための状態を返す。
    bool IsAllRangeMode() const { return isAllRangeMode_; }
    // 使用クラス：GamePlayScene。飛行範囲の内側への自動復帰中かを返す。
    bool IsReturningToFlightArea() const { return isReturningToFlightArea_; }
    // 使用クラス：GamePlayScene。カメラ・噴射・ポストエフェクトの切り替えに使う。
    bool IsBoosting() const { return isBoosting_; }
    float GetForwardSpeedMultiplier() const { return velocity_.z / normalMaxSpeed_; }
    bool IsRolling() const { return isRolling_; }
    void ResetHomingLock() { weaponController_.ResetHomingLock(); }
    float GetManualBank() const { return manualBank_; }
    // 使用クラス：GamePlayScene。キーボード移動とマウス操舵を切り替える。
    void SetControlMode(ControlMode mode);
    // 現在の操作方式を返す。
    ControlMode GetControlMode() const { return controlMode_; }
    // 使用クラス：GamePlayScene。マウス操舵の感度を移動側へ設定する。
    void SetMouseSensitivity(float sensitivity);
    // 現在のマウス操舵の感度を返す。
    float GetMouseSensitivity() const { return mouseSensitivity_; }
    // 使用クラス：GamePlayScene。機首や傾きの演出に使う操舵入力を返す。
    const Vector2& GetStarFoxSteeringInput() const { return steeringInput_; }

    // GamePlaySceneのレティクルとオーバーヒートゲージの配置に使う照準位置を返す。
    const Vector2& GetAimScreenPosition() const { return aimScreenPosition_; }
    // 操縦・射撃の前と、描画用カメラ更新後にパッド照準の距離を補正する。
    void ConstrainGamepadAimToPlayer();
    // 外部から現在の武器の発射を要求する。死亡・過熱・クールダウン中は発射されない。
    void FireBullet(const Camera& activeCamera) { weaponController_.FireBullet(activeCamera); }
    // 使用クラス：GameplayCollisionSystem。プレイヤー弾の衝突判定に使う一覧を返す。
    const std::vector<std::unique_ptr<PlayerBullet>>& GetBullets() const { return weaponController_.GetBullets(); }
    uint64_t GetShotsFired() const { return weaponController_.GetShotsFired(); }
    // 使用クラス：GamePlayScene。生存中の敵をロックオン候補として武器側へ渡す。
    void SetHomingTargets(const std::vector<BaseEnemy*>& targets) { weaponController_.SetHomingTargets(targets); }
    // 敵の破棄前に、発射済みミサイルを含む対象への参照を無効化する。
    void ClearHomingTargets() { weaponController_.ClearHomingTargets(); }
    // 使用クラス：GamePlayScene。ロック表示用の座標を引数の一覧に書き込む。
    void GetHomingLockPositions(std::vector<Vector3>& positions) const { weaponController_.GetHomingLockPositions(positions); }
    // ホーミングミサイルが選択されているかを返す。
    bool IsHomingMissileSelected() const { return weaponController_.IsHomingMissileSelected(); }
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
    bool IsDeathExplosionReady() const { return deathState_ == DeathState::Exploded; }
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

private:
    friend class PlayerWeaponController;
    friend struct PlayerTestAccess;

    // 照準。
    void InitializeAim();
    void UpdateMouseAim();
    // スティック入力を照準の移動量に変換し、入力を離したら少しずつ中央へ戻す。
    void UpdateGamepadAim(float stickX, float stickY, bool isPitchInverted, float deltaTimeSeconds, float sensitivity = 1.0f);
    void ClampAimScreenPosition();
    // 画面上の機体から照準が離れすぎないように制限し、入力の内部位置も揃える。
    void ClampGamepadAimToPlayer(const Vector2& playerScreenPosition);
    // 機体からの照準のずれを、操舵処理が使う画面座標へ変換する。
    Vector2 GetGamepadSteeringScreenPosition() const;
    void CreateAimRay(Ray& aimRay, const Camera& activeCamera) const;
    Vector3 CreateConvergencePoint(const Ray& aimRay) const;
    void UpdateSmoothedAimDistance(const Camera& activeCamera, float deltaTimeSeconds);
    Vector3 ResolveAimPoint(const Ray& aimRay) const;

    // 入力方式はパッドを優先し、その他は設定された操作方式を使う。
    enum class ControlInputType { Keyboard, Mouse, Gamepad };
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
    Vector3 GetWorldVelocity() const { return railForward_ * velocity_.z; }
    // 機首方向・飛行範囲への復帰・高度・位置と傾きを順に更新する。
    void UpdateAllRangeMove(Input* input);
    // 操舵入力で機首の上下角と左右角を変化させ、進行方向を求める。
    void UpdateFlightOrientation(const Vector2& steering, float deltaTimeSeconds);
    // 飛行可能な範囲を超えた場合、範囲内へ戻るための操舵を加える。
    void UpdateFlightAreaReturn(Vector2& steering, float deltaTimeSeconds);
    // 上下の飛行限界を考慮して高度を補正する。
    void UpdateFlightHeight(float deltaTimeSeconds);
    // 進行方向と速度から次の位置を求め、飛行範囲の制限を反映する。
    void UpdateFlightPosition(float deltaTimeSeconds);
    // 旋回入力と手動バンクに合わせて機体を傾ける。
    void UpdateFlightBank(const Vector2& steering, float deltaTimeSeconds);
    // 照準の中心からのずれにデッドゾーンと感度を反映し、操舵入力を滑らかにする。
    void UpdateSteering(const Vector2& aimPosition, const Vector2& screenSizePixels,
        bool isAllRangeMode, float deltaTimeSeconds, bool isGamepad = false);
    void ResetSteering() { steeringInput_ = {}; }
    // 左右のロール開始と進行を処理し、一回転後に通常姿勢へ戻す。
    void UpdateRoll(EulerTransform& transform, bool isLeftTriggered, bool isRightTriggered, float deltaTimeSeconds);
    void StopRoll() { isRolling_ = false; }
    // 機体の大きさとカメラ投影を考慮し、画面からはみ出ないレール上のずれへ補正する。
    Vector3 ClampRailOffsetToScreen(const Vector3& railOffset) const;
    // レールの基準位置に左右・上下のずれを加え、ワールド位置へ変換する。
    Vector3 CalculateRailWorldPosition(const Vector3& railOffset) const;
    // 機体の表示範囲が画面端を越えた量を求める。
    Vector2 CalculateScreenCorrection(const Vector3& railOffset) const;
    // 機体周辺の点を画面へ投影し、表示範囲の最小・最大座標を更新する。
    void UpdateScreenBounds(const Vector3& worldPosition, float& minX, float& maxX, float& minY, float& maxY) const;

    // HP・無敵時間・死亡演出。
    bool ApplyHealthDamage(int damage);
    // 被弾後の無敵時間を減らし、時間切れで通常の被弾状態へ戻す。
    void UpdateInvincibility(float deltaTimeSeconds);
    // 死亡後の落下と回転を進め、爆発へ移れる時点を記録する。
    void UpdateDeathAnimation(EulerTransform& transform, float deltaTimeSeconds);
    bool IsAlive() const { return deathState_ == DeathState::Alive; }
    bool IsFalling() const { return deathState_ == DeathState::Falling; }
    // 死亡状態と被弾中の点滅を調べ、今のフレームで機体を描画するかを返す。
    bool ShouldDraw() const;

    static constexpr float kGamepadMaxAimDistanceRatio = 0.40f;
    const float kAimConvergenceDistance = 220.0f;
    const float kAimDistanceFollowSpeed = 12.0f;
    Vector2 aimScreenPosition_ = { 0.0f, 0.0f };
    Vector2 gamepadAimOffset_ = {};
    Vector2 gamepadAimAnchor_ = {};
    bool hasGamepadAimAnchor_ = false;
    float smoothedAimDistance_ = kAimConvergenceDistance;

    enum class DeathState { Alive, Falling, Exploded };
    int maxHp_ = 20;
    int currentHp_ = maxHp_;
    float invincibleRemainingSeconds_ = 0;
#if defined(ENABLE_DEVELOPMENT_TOOLS)
    bool isInvincibleMode_ = false;
#endif
    static constexpr float kInvincibleDurationSeconds = 1.0f;
    DeathState deathState_ = DeathState::Alive;
    float deathTimerSeconds_ = 0.0f;
    float deathFallVelocity_ = 0.0f;
    static constexpr float kDeathFallDurationSeconds = 1.2f;

    bool isBoosting_ = false;
    ControlMode controlMode_ = ControlMode::StarFox;
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
    float manualBank_ = 0.0f;

    float mouseSensitivity_ = 1.0f; // 中央からの照準のずれに掛ける倍率。
    Vector2 steeringInput_ = {}; // 補間済みの入力。Xは右向き、Yは上向きが正で、中央は0。

    bool isRolling_ = false;
    float rollTimerSeconds_ = 0; // ロール開始からの経過秒数。
    float rollCooldownSeconds_ = 0;
    float rollDirection_ = 0.0f; // 左入力は+1、右入力は-1で、回転方向を切り替える。
    float leftKeyTapTimerSeconds_ = 0;
    float rightKeyTapTimerSeconds_ = 0;
    // 60FPSでの従来の長さを、経過秒数で管理する。
    static constexpr float kMaxTapIntervalSeconds = 0.25f;
    static constexpr float kRollDurationSeconds = 0.5f;
    static constexpr float kRollCooldownDurationSeconds = 1.0f;

    float railMoveLimitX_ = 20.0f;
    float railMoveLimitY_ = 15.0f;
    float railMinimumHeight_ = 0.0f;
    bool hasRailMinimumHeight_ = false;
    // 画面端に残す余白。単位はピクセル。
    float playerClampMarginX_ = 100.0f;
    float playerClampMarginY_ = 100.0f;
    // 補正判定に使う機体の半幅・半高さ。単位はワールド座標。
    float playerBoundsHalfWidth_ = 1.5f;
    float playerBoundsHalfHeight_ = 1.0f;

    // 使用クラス：本クラス。保持している位置・回転・拡大率を描画オブジェクトへ渡す。
    void ApplyTransform();
    // 使用クラス：本クラス（Update）。落下演出と残っている弾の更新を行う。
    void UpdateDeath();

    std::unique_ptr<Object3d> object_;
    // カメラ類は外部所有。参照先はPlayerを使用している間、有効である必要がある。
    Camera* camera_ = nullptr;
    DebugCameraController* debugCameraController_ = nullptr;
    // 機体の位置・回転・大きさを保持し、移動処理と描画で共有する。
    EulerTransform transform_;
    // 発射と弾の管理は、今回の統合対象外として保持する。
    PlayerWeaponController weaponController_{ *this };
    bool isDebugMode_ = false;
};

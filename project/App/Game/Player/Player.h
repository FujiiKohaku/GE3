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

// 自機の描画オブジェクトと状態を保持し、各コントローラーの更新順序を管理する。
class Player {
public:
    using ControlMode = PlayerMovementController::ControlMode;
    Player() = default;
    // 各コントローラーがPlayerのメンバーを参照するため、コピー・ムーブを禁止する。
    Player(const Player&) = delete;
    Player& operator=(const Player&) = delete;
    Player(Player&&) = delete;
    Player& operator=(Player&&) = delete;

    // 使用クラス：GamePlayScene。モデル・武器・移動・照準の初期状態を設定する。
    void Initialize(Model* model);
    // 使用クラス：GamePlayScene。照準・移動・発射・描画姿勢を順に更新する。
    void Update();
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
    void ApplyRailAreaForce(const Vector3& force) { movementController_.ApplyRailAreaForce(force); }
    // 使用クラス：GamePlayScene。レール基準位置からの左右・上下の移動量を返す。
    const Vector3& GetRailOffset() const { return movementController_.GetRailOffset(); }
    // 敵の照準などに使う前進速度を返す。死亡時は0、レール飛行では通常速度を使う。
    Vector3 GetAutomaticWorldVelocity() const;
    // 使用クラス：GamePlaySceneなど。カメラ配置やエフェクトに使う飛行方向を返す。
    const Vector3& GetFlightForward() const { return movementController_.GetFlightForward(); }
    // 使用クラス：GamePlayScene。噴射エフェクトを置く機体後方のワールド位置を返す。
    Vector3 GetEngineExhaustPosition() const;
    // 使用クラス：GamePlayScene。機体の回転に合わせた噴射方向を返す。
    Vector3 GetEngineExhaustDirection() const;

    // 使用クラス：GamePlayScene。指定した飛行範囲で全方向飛行へ切り替える。
    void EnableAllRangeMode(float areaRadius, float minHeight, float maxHeight);
    // レール飛行と全方向飛行を区別するための状態を返す。
    bool IsAllRangeMode() const { return movementController_.IsAllRangeMode(); }
    // 使用クラス：GamePlayScene。飛行範囲の内側への自動復帰中かを返す。
    bool IsReturningToFlightArea() const { return movementController_.IsReturningToFlightArea(); }
    // 使用クラス：GamePlayScene。カメラ・噴射・ポストエフェクトの切り替えに使う。
    bool IsBoosting() const { return movementController_.IsBoosting(); }
    // 使用クラス：GameplayCollisionSystemなど。ロール中かを返す。
    bool IsRolling() const { return movementController_.IsRolling(); }
    // 使用クラス：GamePlayScene。キーボード移動とマウス操舵を切り替える。
    void SetControlMode(ControlMode mode) { movementController_.SetControlMode(mode); }
    // 現在の操作方式を返す。
    ControlMode GetControlMode() const { return movementController_.GetControlMode(); }
    // 使用クラス：GamePlayScene。マウス操舵の感度を移動側へ設定する。
    void SetMouseSensitivity(float sensitivity) { movementController_.SetMouseSensitivity(sensitivity); }
    // 現在のマウス操舵の感度を返す。
    float GetMouseSensitivity() const { return movementController_.GetMouseSensitivity(); }
    // 使用クラス：GamePlayScene。機首や傾きの演出に使う操舵入力を返す。
    const Vector2& GetStarFoxSteeringInput() const { return movementController_.GetStarFoxSteeringInput(); }

    // GamePlaySceneのレティクルとオーバーヒートゲージの配置に使う照準位置を返す。
    const Vector2& GetAimScreenPosition() const { return aimController_.GetAimScreenPosition(); }
    // 外部から現在の武器の発射を要求する。死亡・過熱・クールダウン中は発射されない。
    void FireBullet(const Camera& activeCamera) { weaponController_.FireBullet(activeCamera); }
    // 使用クラス：GameplayCollisionSystem。プレイヤー弾の衝突判定に使う一覧を返す。
    const std::vector<std::unique_ptr<PlayerBullet>>& GetBullets() const { return weaponController_.GetBullets(); }
    // 使用クラス：GamePlayScene。生存中の敵をロックオン候補として武器側へ渡す。
    void SetHomingTargets(const std::vector<BaseEnemy*>& targets) { weaponController_.SetHomingTargets(targets); }
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
    // 使用クラス：GamePlayScene。回復アイテムの回復量をHealthへ渡す。
    bool Heal(int amount) { return health_.Heal(amount); }
    // 使用クラス：GamePlaySceneなど。HPが0以下かを返す。死亡演出の完了とは別の判定。
    bool IsDead() const { return health_.IsDead(); }
    // 使用クラス：GamePlayScene。落下演出後の爆発・シーン遷移へ進めるかを返す。
    bool IsDeathExplosionReady() const { return health_.IsDeathExplosionReady(); }
    // 使用クラス：GamePlayScene。HPのHUD表示と被弾検出に使う現在値を返す。
    int GetCurrentHp() const { return health_.GetCurrentHp(); }
    // 使用クラス：GamePlayScene。HP表示の上限を返す。
    int GetMaxHp() const { return health_.GetMaxHp(); }
#if defined(ENABLE_DEVELOPMENT_TOOLS)
    // 開発用の無敵モードをHealthへ設定する。
    void SetInvincibleMode(bool isEnabled) { health_.SetInvincibleMode(isEnabled); }
    // 開発用の無敵モードが有効かを返す。
    bool IsInvincibleMode() const { return health_.IsInvincibleMode(); }
#endif

    // 使用クラス：GamePlayScene。弾を描画し、Healthの判定に従って機体を描画する。
    void Draw();
    void SubmitRaytracing(DxrRenderer& renderer);
    // 機体の影を描画する。機体の非表示中は影も描かない。
    void DrawShadow(ShadowMapRenderer& renderer);
    // デバッグビルドでHP・武器・熱量を表示する。
    void DrawImGui();

private:
    // 使用クラス：本クラス。保持している位置・回転・拡大率を描画オブジェクトへ渡す。
    void ApplyTransform();
    // 使用クラス：本クラス（Update）。落下演出と残っている弾の更新を行う。
    void UpdateDeath();

    std::unique_ptr<Object3d> object_;
    // カメラ類は外部所有。参照先はPlayerを使用している間、有効である必要がある。
    Camera* camera_ = nullptr;
    DebugCameraController* debugCameraController_ = nullptr;
    // 機体姿勢の本体。Movementはこの値を参照して更新する。
    EulerTransform transform_;
    // Aim＝照準、Movement＝移動、Health＝HPと死亡、Weapon＝発射と弾の管理。
    PlayerAimController aimController_;
    PlayerMovementController movementController_{ transform_, aimController_ };
    PlayerHealth health_;
    PlayerWeaponController weaponController_{ transform_, aimController_, movementController_, health_ };
    bool isDebugMode_ = false;
};

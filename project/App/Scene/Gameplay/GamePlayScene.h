// ステージ進行の中心。プレイヤー・敵・ボス・背景・HUD・演出を組み合わせて更新する。
#pragma once
#if defined(ENABLE_DEVELOPMENT_TOOLS)
#include "externals/json.hpp"
#endif
#include "Engine/3D/SkinningObject3d.h"
#include "Engine/3D/SkinningObject3dManager.h"

#include "Engine/Animation/PlayAnimation.h"
#include "Engine/Skeleton/Skeleton.h"

#include "App/Scene/Common/BaseScene.h"
#include "Pause/BasePauseState.h"
#include "Engine/2D/Text/Text.h"
#include "Engine/2D/Text/TextRenderer.h"

#include "Engine/Camera/Camera.h"
#include "Engine/debugcamera/DebugCameraController.h"

#include "Engine/3D/ModelManager.h"
#include "Engine/3D/Object3d.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/3D/OceanSurface.h"
#include "Engine/3D/WaterPillarRenderer.h"

#include "Engine/Effect/EffectManager.h"
#include "Engine/Rail/Rail.h"

#include "Engine/audio/SoundManager.h"

#include "Engine/2D/Sprite.h"
#include "Engine/2D/SpriteManager.h"
#include "Engine/TextureManager/TextureManager.h"
#include <numbers>


#include "App/Game/Player/Player.h"
#include "App/Game/Collision/GameplayCollisionSystem.h"
#include "App/Game/Stage/StageCatalog.h"
#include "App/Game/Stage/NormalEnemyLevelTable.h"
#include "App/Game/Tutorial/TutorialCommPanel.h"
#include "App/Game/Tutorial/TutorialController.h"
#include "App/Game/Hazard/WaterPillarHazard.h"
#include "Engine/3D/SkyBox/SkyBox.h"
#include "Engine/3D/SkyBox/SkyBoxManager.h"
#include "Engine/postEffect/CopyImageRenderer.h"
#include <memory>
#include <string>
#include <vector>

#include "Engine/EditorManager/EditorManager.h"

#include "Engine/SceneObjectManager/SceneObjectManager.h"

#include "App/Game/Enemy/BaseEnemy.h"
#include "App/Game/Enemy/Bullet/EnemyBulletManager.h"

#include "App/Game/Enemy/NormalEnemy/NormalEnemy.h"
#include "App/Game/Enemy/ArmoredEnemy/ArmoredEnemy.h"
#include "App/Game/Enemy/SwarmEnemy/SwarmEnemy.h"
#include "App/Game/Boss/Encounter/BossEncounterController.h"
#include "App/Game/Boss/IceJellyfish/IceJellyfish.h"
#include "App/Game/Boss/Common/BaseBoss.h"

struct LevelData;

class GamePlayScene : public BaseScene {
public:
    explicit GamePlayScene(const std::string& stageId = "stage01")
        : stageId_(stageId) {}

    // ステージ設定を読み、機体・敵・ボス・背景・UIと演出をプレイ開始状態へ揃える。
    void Initialize() override;

    // プレイ専用の演出や外部登録を終了し、シーンのリソースを片付ける。
    void Finalize() override;

    // ポーズや演出の状態を確認し、レール・機体・敵・判定・カメラ・UIを更新する。
    void Update() override;

    // 照準・HP・得点・通信パネルなど、画面に重ねる情報を描画する。
    void Draw2D() override;
    // 背景・ステージ・機体・敵などの立体物を、各描画経路へ渡す。
    void Draw3D() override;
    // レイトレーシングに参加する立体物を登録する。通常描画の可視判定とは別に扱う。
    bool SubmitRaytracingScene(DxrRenderer& renderer) override;
    void DrawShadow(ShadowMapRenderer& renderer) override;
    ShadowSettings GetShadowSettings() const override { return stageSettings_.shadows; }
    void DrawParticle() override;
    void DrawImGui() override;
#if defined(ENABLE_DEVELOPMENT_TOOLS)
    // 開発用パネルへこのシーンを登録し、内部状態の確認や設定変更を受け付ける。
    void RegisterDevelopmentPanel();
    // 開発用パネルに表示する操作項目をJSONとして組み立てる。
    nlohmann::json GetDevelopmentControls() const;
    // 開発パネルからの真偽値変更を、共通の設定反映処理へ渡す。
    bool SetDevelopmentBool(const std::string& key, bool isEnabled);
    // 開発パネルからの数値変更を、共通の設定反映処理へ渡す。
    bool SetDevelopmentNumber(const std::string& key, double value);
    // 開発パネルで指定されたコマンドを実行する。
    bool ExecuteDevelopmentCommand(const std::string& key);
    // シーンの現在状態を、開発パネル向けのJSONへまとめる。
    nlohmann::json GetDevelopmentState() const;
    // シーンの開発用状態をJSON文字列に変換する。
    std::string GetDevelopmentStateJson() const;
    // 指定されたキーと値を調べ、対応する検証用設定や操作へ反映する。
    bool ApplyDevelopmentAction(const std::string& key, const std::string& value);
#endif

private:
    // ボス登場時のカットインに使う画像・文字・進行状態を準備する。
    void InitializeBossCutIn();
    // ボス登場の通知からカットインを開始し、表示と時間演出を進める。
    void UpdateBossCutIn();
    // カットインで変更した時間の進み方を元に戻す。
    void RestoreBossCutInTimeScale();
    static constexpr float kBossCutInDurationSeconds = 0.65f;
    static constexpr float kBossCutInSlowDurationSeconds = 0.40f;
    std::unique_ptr<Sprite> bossCutInSprite_;
    float bossCutInTimerSeconds_ = -1.0f;
    float bossCutInPreviousTimeScale_ = 1.0f;
    bool hasPlayedBossCutIn_ = false;
    bool isBossCutInSlowMotionActive_ = false;
#if defined(KOHAKU_RENDER_TESTS)
    friend class ShadowTestStage;
#endif
    // プレイヤーと敵の当たり判定
    static constexpr float kPlayerEnemyCollisionRadius = 2.0f;
    static constexpr float kPlayerBulletEnemyCollisionRadius = 4.0f;

    // ブースト演出
    static constexpr float kBoostPostEffectBaseWeight = 0.40f;
    static constexpr float kBoostPostEffectVanishPointWeight = 0.30f;
    static constexpr float kBoostPostEffectPlayerWeight = 0.30f;
    static constexpr float kBoostPostEffectCenterMin = 0.28f;
    static constexpr float kBoostPostEffectCenterMax = 0.72f;
    static constexpr float kBoostPostEffectVanishPointDistance = 150.0f;
    static constexpr float kBoostPostEffectCenterLerpRate = 0.1f;
    static constexpr float kBoostKickDuration = 0.2f;
    static constexpr float kBoostKickFovAdd = 0.1f;

    // カメラシェイク
    static constexpr float kCameraShakeFadeDuration = 5.0f;
    static constexpr float kPlayerDamageShakeDuration = 0.35f;
    static constexpr float kPlayerDamageShakeStrength = 0.006f;
    static constexpr float kBossMadShakeDuration = 7.0f;
    static constexpr float kBossMadShakeStrength = 0.008f;
    static constexpr float kBossBeamShakeDuration = 0.1f;
    static constexpr float kBossBeamShakeStrength = 0.0015f;

    // 回復アイテム
    static constexpr float kRecoveryItemCollisionRadius = 3.0f;
    static constexpr float kRecoveryItemRotationSpeed = 0.035f;
    static constexpr float kRecoveryItemBobSpeed = 0.045f;
    static constexpr float kRecoveryItemBobHeight = 0.65f;
    static constexpr int32_t kRecoveryItemHealAmount = 5;

    // スウォーム
    static constexpr int32_t kSwarmMembersPerWave = 18;

    // 水柱ヒット時の画面水滴
    static constexpr float kWaterDropEffectDuration = 4.0f;

    std::string stageId_ = "stage01";
    StageSettings stageSettings_;

    // 配置データの種類に応じてモデル・障害物・トリガーなどを生成する。
    void CreateLevelObjects(const LevelData& levelData);
    // レベルから生成したオブジェクトと、それに付随する判定情報を破棄する。
    void ClearLevelObjects();
    // 開発中の配置データを再読み込みし、レベル内のオブジェクトを作り直す。
    void HotReloadLevel();
    // ステージの配置情報を、敵や編隊の出現条件へ変換する。
    void LoadEnemyPopData(const LevelData& levelData);
    // 衝突システムの結果を受け取り、得点・ダメージ演出・ステージイベントへ反映する。
    void CheckCollision();
    // ペイント弾の命中を受けて、画面に付着したような表示を開始する。
    void StartPaintHitEffect();
    // 水滴が画面に付く演出を開始する。
    void StartWaterDropEffect();
    // 水滴演出の経過時間と表示の強さを更新する。
    void UpdateWaterDropEffect();
#if defined(_DEBUG) || defined(ENABLE_DEVELOPMENT_TOOLS)
    // 開発中に障害物・敵・トリガーの当たり判定を可視化する。
    void DrawCollisionDebug();
#endif
    // 少し先のレール位置から進行方向を求め、向きが求められない場合の代替方向も用意する。
    Vector3 CalculateRailForward(float distance, const Vector3& railPosition) const;
    // 進行方向をもとに、レール上の左右・上下を表す直交方向を作る。
    void CalculateRailBasis(const Vector3& forward, Vector3& right, Vector3& up) const;
    // エンカウンターが所有しているボスを参照する。シーン自身はボスを所有しない。
    BaseBoss* GetActiveBoss() const;
    // 氷ステージ向けに敵の外見と描画設定を調整する。
    void ConfigureFrozenEnemyAppearance(BaseEnemy& enemy);

    // Updateメソッドの処理分割用ヘルパー関数
    void UpdateRailMovement(Vector3& outPosition, Vector3& outForward, Vector3& outRight, Vector3& outUp, float& outNextDistance);
    // レール上の基準姿勢とプレイヤー操作を合わせて機体の位置・向きを更新する。
    void UpdatePlayerTransform(const Vector3& currentPosition, const Vector3& railRight, const Vector3& railUp, const Vector3& forward);
    // 機体とレールに追従する視点を作り、加減速や旋回によるカメラ演出を加える。
    void UpdateCamera(const Vector3& currentPosition, const Vector3& forward, const Vector3& railRight, const Vector3& railUp, float nextRailDistance, Input* input);
    // ブースト開始時の勢いを示す一時的なカメラ変化を更新する。
    void UpdateBoostKick(bool isPlayerBoosting);
    // 減速の開始と継続に合わせて、視点や機体の反応を調整する。
    void UpdateBrakeReaction(bool isPlayerBraking);
    // レールの進行先をもとに、ブーストの放射状効果の中心を更新する。
    void UpdateBoostPostEffectCenter(float nextRailDistance, bool isPlayerBoosting);
    // 被弾や演出による揺れの強さを更新し、画面効果に反映する。
    void UpdateCameraShakePostEffect();
    // 少し先の進行方向を画面へ投影し、速度演出を集める中心位置を求める。
    Vector2 CalculateBoostPostEffectCenter(float nextRailDistance) const;
    // ブーストなどの状態に応じて、プレイ中の画面効果を組み合わせる。
    void ConfigureGameplayPostEffects(bool isPlayerBoosting);
    // 武器の熱量から画面端の演出強度を計算し、過熱や冷却の変化を滑らかに見せる。
    void UpdateWeaponHeatPostEffect();
    // ステージに指定された照明・霧・画面効果のプリセットを適用する。
    void ApplyStageVisualPreset();
    // 開発用パネルで指定した照明設定を描画へ反映する。
    void ApplyDevelopmentLighting();
    // プレイ専用の画面効果を解除し、次の画面へ設定を残さないようにする。
    void ResetGameplayPostEffects();
    // 機体に追従している噴射エフェクトを停止し、参照を解除する。
    void StopPlayerEngineEffects();
    // ボスの部位HPを表示用の割合へ変換し、HPバーの表示状態を更新する。
    void UpdateBossHpHud();
    // レールの進行距離を確認し、未出現の群れ編隊を必要なタイミングで出す。
    void UpdateSwarmWaveSpawning();
    // 編隊設定から各個体の担当位置を決め、群れの敵を生成する。
    void SpawnSwarmWave(
        SwarmFormationType formationType,
        int32_t travelDirection);
    // 回復アイテムのモデルと配置状態を準備する。
    void InitializeRecoveryItems(Model* model);
    // 回復アイテムの見た目とプレイヤーとの接触を更新する。
    void UpdateRecoveryItems();
    // 海中や海上の生き物を、背景演出用の位置へ配置する。
    void InitializeOceanLife();
    // ステージ内で出現する水柱の配置と出現条件を準備する。
    void InitializeWaterPillars();
    // 進行位置と時間に合わせて、背景の生き物を動かす。
    void UpdateOceanLife(const Vector3& railPosition, const Vector3& forward, const Vector3& railRight);

    std::unique_ptr<SceneObjectManager> sceneObjectManager_;
    std::unique_ptr<GameplayCollisionSystem> gameplayCollisionSystem_;

    int test_ = 0;
    std::unique_ptr<EditorManager> editorManager_;
    std::unique_ptr<Rail> rail_;
    float railDistance_ = 0.0f;
    float railSpeed_ = 0.5f;
    float railDirectionSampleDistance_ = 5.0f;
    float cameraLookAheadDistance_ = 30.0f;
    // ------------------------------
    // カメラ / デバッグカメラコントローラー
    // ------------------------------
    std::unique_ptr<Camera> camera_;
    std::unique_ptr<DebugCameraController> debugCameraController_;
    // ------------------------------
    // 3dオブジェクト
    // ------------------------------

    // std::unique_ptr<Object3d> terrain_;
    // std::unique_ptr<Object3d> plane_;
    std::unique_ptr<Object3d> floorObj_;
    std::unique_ptr<OceanSurface> oceanSurface_;
    std::unique_ptr<WaterPillarRenderer> waterPillarRenderer_;
    std::vector<std::unique_ptr<Object3d>> oceanFish_;
    std::vector<std::unique_ptr<Object3d>> oceanBirds_;
    std::vector<std::unique_ptr<WaterPillarHazard>> waterPillars_;
    float oceanLifeTime_ = 0.0f;
    float fishSchoolTimer_ = 0.0f;
    float fishSchoolCooldown_ = 5.0f;
    bool isFishSchoolActive_ = false;
    bool fishSchoolFromLeft_ = true;
    std::unique_ptr<Object3d> droneObj_;
    std::unique_ptr<SkyBox> skyBox_;
    std::unique_ptr<SkinningObject3d> skinningPlayer_;
    std::vector<std::unique_ptr<Object3d>> levelObjects_;
    std::vector<DestructibleLevelObject> destructibleLevelObjects_;
    std::vector<StageTrigger> stageTriggers_;

    struct RecoveryItem {
        std::unique_ptr<Object3d> object;
        Vector3 basePosition = { 0.0f, 0.0f, 0.0f };
        EffectHandle effectHandle = kInvalidEffectHandle;
        float animationTime = 0.0f;
        bool collected = false;
    };
    std::vector<RecoveryItem> recoveryItems_;

    // player
    std::unique_ptr<Player> player_;

    // ------------------------------
    // 2dオブジェクト
    // ------------------------------
    std::unique_ptr<Sprite> testSprite_;
    std::unique_ptr<Sprite> aimSprite_;
    std::unique_ptr<Sprite> overheatGaugeSprite_;
    std::vector<std::unique_ptr<Sprite>> homingLockSprites_;
    std::unique_ptr<Sprite> weaponHudBgSprite_;
    std::vector<std::unique_ptr<Sprite>> weaponHudFrameSprites_;
    std::vector<std::unique_ptr<Sprite>> playerHudFrameSprites_;
    std::vector<std::unique_ptr<Sprite>> bossHudFrameSprites_;
    std::unique_ptr<Text> weaponHudLabelText_;
    std::unique_ptr<Text> weaponHudNameText_;
    uint32_t score_ = 0;
    std::unique_ptr<Sprite> scoreHudBgSprite_;
    std::vector<std::unique_ptr<Sprite>> scoreHudFrameSprites_;
    std::unique_ptr<Text> scoreHudLabelText_;
    std::unique_ptr<Text> scoreHudValueText_;
    // ------------------------------
    // BGM / SE
    // ------------------------------
    SoundData bgm;

    // ------------------------------
    // Light
    // ------------------------------
    bool sphereLighting = true;
    float lightIntensity = 1.0f;
    Vector3 lightDir = { 0.0f, -1.0f, 0.0f };

    // ------------------------------
    // transform
    // ------------------------------
    Vector3 spherePos = { 0.0f, 0.0f, 0.0f };
    Vector3 sphereRotate = { 0.0f, 0.0f, 0.0f };
    Vector3 sphereScale = { 1.0f, 1.0f, 1.0f };

    // ------------------------------
    // Terrain Transform
    // ------------------------------
    Vector3 terrainPos = { 0.0f, -10.0f, 0.0f };
    Vector3 terrainRotate = { 0.0f, 0.0f, 0.0f };
    Vector3 terrainScale = { 1.0f, 1.0f, 1.0f };
    // ------------------------------
    // Plane Transform
    // ------------------------------
    Vector3 planePos = { 0.0f, 0.0f, 0.0f };
    Vector3 planeRotate = { 0.0f, std::numbers::pi_v<float>, 0.0f };
    Vector3 planeScale = { 1.0f, 1.0f, 1.0f };
    // ------------------------------
    // playerTransform
    // ------------------------------
    float r = 0.0f;

    Vector3 cameraRotate_ = { 0.0f, 0.0f, 0.0f };
    EffectHandle playerJetHandle_ = kInvalidEffectHandle;
    EffectHandle playerJetSparkHandle_ = kInvalidEffectHandle;
    bool wasPlayerBoosting_ = false;
    bool wasPlayerBraking_ = false;
    float brakeReactionStrength_ = 0.0f;
    float brakeFovStrength_ = 0.0f;
    static constexpr float kBrakeCameraKickDurationSeconds = 0.32f;
    float brakeCameraKickRemainingSeconds_ = 0.0f;
    bool wasBoostingForKick_ = false;
    float normalFovY_ = 0.45f;
    float boostFovY_ = 0.60f;
    float currentFovY_ = 0.45f;
    float boostKickTimer_ = 0.0f;
    float boostKickStrength_ = 0.0f;
    float fovLerpRate_ = 0.1f;
    float cameraFollowLerpRate_ = 0.2f;
    float cameraForwardLerpRate_ = 0.2f;
    bool hasCameraFollowState_ = false;
    Vector3 smoothedCameraPosition_ = { 0.0f, 0.0f, 0.0f };
    Vector3 smoothedLookAheadPosition_ = { 0.0f, 0.0f, 0.0f };
    Vector3 smoothedCameraForward_ = { 0.0f, 0.0f, 1.0f };
    Vector2 smoothedBoostPostEffectCenter_ = { 0.5f, 0.5f };
    float weaponHeatEdgeIntensity_ = 0.0f;
    NormalEnemyLevelTable normalEnemyLevels_;
    float weaponOverheatFlashRemainingSeconds_ = 0.0f;
    float weaponRecoveryFlashRemainingSeconds_ = 0.0f;
    bool wasWeaponOverheated_ = false;
    EffectHandle playerHeatSteamHandle_ = kInvalidEffectHandle;
    Vector3 cameraOffset_ = {
        0.0f,
        5.0f,
        -30.0f
    };

    float followX_ = 0.35f;
    float followY_ = 0.35f;
    // エネミー配列
    std::vector<std::unique_ptr<BaseEnemy>> enemies_;
    EnemyBulletManager enemyBulletManager_;
    size_t nextSwarmWaveIndex_ = 0;

    // エイム用仮想カメラ
    std::unique_ptr<Camera> aimCamera_;

    // エネミー・弾モデル
    Model* enemyModel_ = nullptr;
    Model* enemyBulletModel_ = nullptr;
    Model* fearWormEnemyModel_ = nullptr;

    // カメラオフセット定数
    static constexpr float kCameraBackwardOffset = 35.0f;
    // EXZODIAC-like low chase view: keep the eye slightly below the rail
    // center so the player is framed against the horizon instead of seen
    // from above.
    static constexpr float kCameraUpwardOffset = 1.0f;
    static constexpr float kAllRangeCameraUpwardOffset = 2.0f;

    // カメラパラメータ (プレイヤー上下移動連動用)
    float cameraHeightFollowFactor_ = 0.3f;
    float cameraLookUpFactor_ = 0.595f;
    float cameraHorizontalFollowFactor_ = 0.2f;
    float cameraLookHorizontalFactor_ = 0.35f;

    // ボス戦用
    std::unique_ptr<BossEncounterController> bossController_;
    bool showIceJellyfishCollision_ = true;
    bool lightEnabled_ = true;
    Vector4 lightColor_ = { 0.90f, 0.96f, 1.0f, 1.0f };
    float lightIntensity_ = 0.90f;
    Vector3 lightDir_ = { -0.35f, -0.82f, 0.45f };
    Vector4 ambientColor_ = { 0.40f, 0.52f, 0.68f, 0.24f };
    bool pointEnabled_ = true;
    Vector4 pointColor_ = { 1.0f, 1.0f, 1.0f, 1.0f };
    Vector3 pointPos_ = { 0.0f, 2.0f, 0.0f };
    float pointIntensity_ = 1.0f;
    float pointRadius_ = 10.0f;
    float pointDecay_ = 1.0f;
    bool spotEnabled_ = true;
    Vector4 spotColor_ = { 1.0f, 1.0f, 1.0f, 1.0f };
    Vector3 spotPos_ = { 0.0f, 0.0f, 0.0f };
    Vector3 spotDir_ = { -1.0f, 0.0f, 0.0f };
    float spotIntensity_ = 4.0f;
    float spotDistance_ = 7.0f;
    float spotDecay_ = 2.0f;
    float spotAngleDeg_ = 60.0f;
    float spotFalloffStartDeg_ = 30.0f;
#if defined(_DEBUG) || defined(ENABLE_DEVELOPMENT_TOOLS)
    bool developmentPaused_ = false;
    bool stepDevelopmentFrame_ = false;
    bool showRailDebug_ = false;
    bool showCollisionDebug_ = false;
    bool showPlayerCollision_ = true;
    bool showEnemyCollision_ = true;
    bool showStageCollision_ = true;
    bool showBulletCollision_ = false;
    bool collisionOverlay_ = true;
    float collisionDrawDistance_ = 500.0f;
#endif

    // カメラシェイク演出用
    float cameraShakeTime_ = 0.0f;
    float cameraShakeStrength_ = 0.0f;
    float cameraShakeDuration_ = 0.0f;

    // ペイントポストエフェクト用
    bool isPaintEffectActive_ = false;
    float paintEffectTimer_ = 0.0f;
    float paintEffectDuration_ = 5.5f;

    // 水柱ヒット時の画面水滴用
    float waterDropEffectTimer_ = 0.0f;

    // 被弾フラッシュ演出用
    float damageFlashTimer_ = 0.0f;
    int lastPlayerHp_ = 20;

    // プレイヤー爆発後、ゲームオーバー画面へ移るまでの待機時間
    float playerDeathAfterExplosionTimer_ = 0.0f;
    bool playerDeathExplosionPlayed_ = false;
    bool hasPlayerDeathCameraState_ = false;
    Vector3 playerDeathCameraOffset_ {};
    Vector3 playerDeathCameraLookTarget_ {};

    // ボス撃破ディゾルブ用タイマー
    float bossDeathDissolveTimer_ = 0.0f;

    // ブースト加速ソニックブーム衝撃音波タイマー
    float sonicBoomTimer_ = 0.0f;

    // ポーズメニュー（TABキー）関連
    bool isPaused_ = false;
    int pauseSelection_ = 0;
    const BasePauseState* pauseState_ = &GetPauseState(PauseViewState::Menu);
    // ポーズ画面が参照・変更する値と操作を、一つの情報にまとめて渡す。
    PauseStateContext MakePauseContext();
    // ポーズ画面の表示先と選択位置を切り替える。
    void ChangePauseState(PauseViewState state, int selection = 0);
    // ポーズ画面で選択中の項目を決定する。
    void ActivatePauseSelection();
    // 設定画面から操舵感度を変更し、許可された範囲へ収める。
    void AdjustPauseSensitivity(float amount);
    // 訓練の開始・成功・完了を示す表示と演出を準備する。
    void InitializeTutorialPresentation();
    // チュートリアルの進行に合わせて案内を切り替え、開始や完了の演出を進める。
    bool UpdateTutorialPresentation();
    std::unique_ptr<Text> tutorialTitleText_;
    std::unique_ptr<TutorialCommPanel> tutorialCommPanel_;
    TutorialController tutorialController_;
    std::shared_ptr<int> tutorialDefeatedTargets_;
    int tutorialTargetStep_ = -1;
    float tutorialGateDistance_ = -1.0f;
    std::unique_ptr<Text> tutorialSkipText_;
    std::unique_ptr<Sprite> tutorialExitOverlay_;
    float tutorialExitTime_ = -1.0f;
    float tutorialIntroTime_ = 0.0f;
    bool tutorialExitQueued_ = false;
    std::unique_ptr<Sprite> pauseMenuPanelSprite_;
    std::unique_ptr<Sprite> pauseResumeBtnSprite_;
    std::unique_ptr<Sprite> pauseRetryBtnSprite_;
    std::unique_ptr<Sprite> pauseTitleBtnSprite_;
    std::unique_ptr<Sprite> pauseControlBtnSprite_;

    // ポーズ用日本語テキストUI（Textクラス）
    std::unique_ptr<Text> pauseTitleText_;
    std::unique_ptr<Text> pauseResumeText_;
    std::unique_ptr<Text> pauseRetryText_;
    std::unique_ptr<Text> pauseTitleBtnText_;
    std::unique_ptr<Text> pauseControlText_;
    std::unique_ptr<Text> pauseSensitivityText_;

    // 画面右側のプレイヤーHPゲージUI
    std::unique_ptr<Sprite> playerHpBgSprite_;
    std::unique_ptr<Sprite> playerHpBarSprite_;
    std::unique_ptr<Text> playerHpText_;
    float displayedPlayerHpRatio_ = 1.0f;

    // Release構成でも表示するボスHP HUD
    std::unique_ptr<Sprite> bossHeadHpBgSprite_;
    std::unique_ptr<Sprite> bossHeadHpBarSprite_;
    std::unique_ptr<Sprite> bossBodyHpBgSprite_;
    std::unique_ptr<Sprite> bossBodyHpBarSprite_;
    std::unique_ptr<Text> bossNameText_;
    std::unique_ptr<Text> bossHeadHpText_;
    std::unique_ptr<Text> bossBodyHpText_;
    float displayedBossHeadHpRatio_ = 1.0f;
    float displayedBossBodyHpRatio_ = 1.0f;

    uint32_t bossSpotLightHandle_ = 0xffffffffu;
    uint32_t bossPointLightHandle_ = 0xffffffffu;
    bool hasCameraPoint_ = false;
    LevelData::ObjectData cameraPointObject_ {};
    float cameraPointLerpTime_ = 0.0f;
};

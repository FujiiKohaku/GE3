#pragma once
#include "Engine/3D/SkinningObject3d.h"
#include "Engine/3D/SkinningObject3dManager.h"

#include "Engine/Animation/PlayAnimation.h"
#include "Engine/Skeleton/Skeleton.h"

#include "App/Scene/BaseScene.h"
#include "Engine/2D/Text/Text.h"
#include "Engine/2D/Text/TextRenderer.h"

#include "Engine/Camera/Camera.h"
#include "Engine/debugcamera/DebugCameraController.h"

#include "Engine/3D/ModelManager.h"
#include "Engine/3D/Object3d.h"
#include "Engine/3D/Object3dManager.h"

#include "Engine/Effect/EffectManager.h"
#include "Engine/Rail/Rail.h"

#include "Engine/audio/SoundManager.h"

#include "Engine/2D/Sprite.h"
#include "Engine/2D/SpriteManager.h"
#include "Engine/TextureManager/TextureManager.h"
#include <numbers>

#include "Engine/Animation/AnimationActor.h"

#include "App/Game/Player/Player.h"
#include "Engine/3D/SkyBox/SkyBox.h"
#include "Engine/3D/SkyBox/SkyBoxManager.h"
#include "Engine/postEffect/CopyImageRenderer.h"
#include <memory>
#include <vector>

#include "Engine/EditorManager/EditorManager.h"

#include "Engine/SceneObjectManager/SceneObjectManager.h"

#include "App/Game/Enemy/BaseEnemy.h"

#include "App/Game/Enemy/Types/NormalEnemy.h"
#include "App/Game/Boss/FearWormEnemy/FearWormEnemy.h"

struct LevelData;

class GamePlayScene : public BaseScene {
public:
    void Initialize() override;

    void Finalize() override;

    void Update() override;

    void Draw2D() override;
    void Draw3D() override;
    void DrawParticle() override;
    void DrawImGui() override;

private:
    void CreateLevelObjects(const LevelData& levelData);
    void ClearLevelObjects();
    void HotReloadLevel();
    void LoadEnemyPopData(const LevelData& levelData);
    void CheckCollision();
#ifdef _DEBUG
    void DrawCollisionDebug();
#endif
    Vector3 CalculateRailForward(float distance, const Vector3& railPosition) const;
    void CalculateRailBasis(const Vector3& forward, Vector3& right, Vector3& up) const;

    // Updateメソッドの処理分割用ヘルパー関数
    void UpdateRailMovement(Vector3& outPosition, Vector3& outForward, Vector3& outRight, Vector3& outUp, float& outNextDistance);
    void UpdatePlayerTransform(const Vector3& currentPosition, const Vector3& railRight, const Vector3& railUp, const Vector3& forward);
    void UpdateCamera(const Vector3& currentPosition, const Vector3& forward, const Vector3& railRight, const Vector3& railUp, float nextRailDistance, Input* input);
    void UpdateBoostKick(bool isPlayerBoosting);
    void UpdateBoostPostEffectCenter(float nextRailDistance, bool isPlayerBoosting);
    Vector2 CalculateBoostPostEffectCenter(float nextRailDistance) const;
    void ResetGameplayPostEffects();
    void ProcessPlayerShooting(Input* input);

    std::unique_ptr<SceneObjectManager> sceneObjectManager_;

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
    std::unique_ptr<Object3d> droneObj_;
    std::unique_ptr<SkyBox> skyBox_;
    std::unique_ptr<SkinningObject3d> skinningPlayer_;
    std::unique_ptr<AnimationActor> animationActor_;
    std::vector<std::unique_ptr<Object3d>> levelObjects_;
    // player
    std::unique_ptr<Player> player_;

    // ------------------------------
    // 2dオブジェクト
    // ------------------------------
    std::unique_ptr<Sprite> testSprite_;
    std::unique_ptr<Sprite> aimSprite_;
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
    EffectHandle boostLineHandle_ = kInvalidEffectHandle;
    bool wasPlayerBoosting_ = false;
    bool wasBoostingForKick_ = false;
    bool isRandomPostEffect_ = false;
    bool hasRandomPostEffectToggle_ = false;
    float normalFovY_ = 0.45f;
    float boostFovY_ = 0.75f;
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
    Vector3 cameraOffset_ = {
        0.0f,
        5.0f,
        -30.0f
    };

    float followX_ = 0.35f;
    float followY_ = 0.35f;
    // エネミー配列
    std::vector<std::unique_ptr<BaseEnemy>> enemies_;

    // エイム用仮想カメラ
    std::unique_ptr<Camera> aimCamera_;

    // エネミー・弾モデル
    Model* enemyModel_ = nullptr;
    Model* enemyBulletModel_ = nullptr;
    Model* fearWormEnemyModel_ = nullptr;

    // カメラオフセット定数
    static constexpr float kCameraBackwardOffset = 35.0f;
    static constexpr float kCameraUpwardOffset = 6.0f;

    // カメラパラメータ (プレイヤー上下移動連動用)
    float cameraHeightFollowFactor_ = 0.3f;
    float cameraLookUpFactor_ = 0.7f;

    // ボス戦用
    std::unique_ptr<FearWormEnemy> activeBoss_;
    bool isBossSpawned_ = false;

    // カメラシェイク演出用
    float cameraShakeTime_ = 0.0f;
    float cameraShakeIntensity_ = 0.0f;
    float cameraShakeDuration_ = 0.0f;

    // ペイントポストエフェクト用
    bool isPaintEffectActive_ = false;
    float paintEffectTimer_ = 0.0f;
    float paintEffectDuration_ = 5.5f;

    // 被弾フラッシュ演出用
    float damageFlashTimer_ = 0.0f;
    int lastPlayerHp_ = 20;

    // ボス登場時電波障害ノイズ用フェードアウトタイマー
    float bossNoiseFadeTimer_ = 0.0f;

    // プレイヤー死亡ディゾルブ用タイマー
    float playerDeathDissolveTimer_ = 0.0f;

    // ボス撃破ディゾルブ用タイマー
    float bossDeathDissolveTimer_ = 0.0f;

    // ボス無差別弾バラマキ衝撃波タイマー
    float bossShockwaveTimer_ = 0.0f;

    // ポーズメニュー（TABキー）関連
    bool isPaused_ = false;
    std::unique_ptr<Sprite> pauseMenuPanelSprite_;
    std::unique_ptr<Sprite> pauseResumeBtnSprite_;
    std::unique_ptr<Sprite> pauseRetryBtnSprite_;
    std::unique_ptr<Sprite> pauseTitleBtnSprite_;

    // ポーズ用日本語テキストUI（Textクラス）
    std::unique_ptr<Text> pauseTitleText_;
    std::unique_ptr<Text> pauseResumeText_;
    std::unique_ptr<Text> pauseRetryText_;
    std::unique_ptr<Text> pauseTitleBtnText_;

    // 画面右側のプレイヤーHPゲージUI
    std::unique_ptr<Sprite> playerHpBgSprite_;
    std::unique_ptr<Sprite> playerHpBarSprite_;
    std::unique_ptr<Text> playerHpText_;

    const LevelData::ObjectData* cameraPointObject_ = nullptr;
    float cameraPointLerpTime_ = 0.0f;
};

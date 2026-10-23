// モデル・アニメーション・足の接地補正・エフェクトなどを確認するための開発用シーン。
#pragma once
#if defined(ENABLE_DEVELOPMENT_TOOLS)
#include "externals/json.hpp"
#endif
#include "App/Scene/Common/BaseScene.h"
#include "App/Scene/Common/SceneManager.h"
#include "Engine/Camera/Camera.h"
#include "Engine/Effect/EffectManager.h"
#include "Engine/debugcamera/DebugCameraController.h"
#include "Engine/3D/Object3d.h"
#include "Engine/Animation/AnimationActor.h"
#include "Engine/Animation/Animation.h"
#include "Engine/Math/MathStruct.h"
#include "Engine/2D/Text/Text.h"
#include <array>
#include <cstddef>
#include <memory>
#include <numbers>
#include <vector>

class TestScene1 : public BaseScene {
public:
    // 検証対象のモデル・地形・アニメーションと、操作・描画の設定を準備する。
    void Initialize() override;
    // 検証用のエフェクトやモデル、外部登録を終了する。
    void Finalize() override;
    // 開発用の入力を処理し、モデルの移動・アニメーション・足の接地補正と演出を更新する。
    void Update() override;
    void Draw2D() override;
    void Draw3D() override;
    bool SubmitRaytracingScene(DxrRenderer& renderer) override;
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
    // 開発画面で選択された画面効果とパラメーターを適用する。
    void ApplySelectedPostEffect();
    // 砂ゴーレムの各ボーンを現在の検証設定に合わせて配置する。
    void UpdateSandGolemSkeletonPose();
    // 左右の足が地面に沿うように、地形の高さと傾きをボーンへ反映する。
    void ApplyFootIK();
    // 腰側・膝・足先の関係から、目標位置に足を置くための脚の姿勢を計算する。
    void SolveLegIK(
        const std::string& upperLegName,
        const std::string& lowerLegName,
        const std::string& footName,
        const Vector3& worldTarget,
        const Vector3& worldNormal,
        float ikWeight);
    // 指定した位置付近の地形を調べ、接地位置と面の向きを求める。
    bool SampleTerrainSurface(
        float worldX,
        float worldZ,
        float& worldHeight,
        Vector3& worldNormal) const;
    // 足裏の複数地点から地形を調べ、足を置く面の位置と向きを求める。
    bool SampleFootSoleSurface(
        const Vector3& footJointPosition,
        float& worldHeight,
        Vector3& worldNormal) const;
    // 手などの取り付け先の姿勢から、刀の位置と向きを更新する。
    void UpdateKatanaAttachment();
    // アニメーション内のイベントを受け取り、攻撃や効果音・エフェクトの開始へつなげる。
    void ProcessAnimationEvents();
    // 移動状態に応じて砂煙などの追従エフェクトを更新する。
    void UpdateMovementEffects();
    // ダッシュ開始の瞬間にだけ、勢いを示すエフェクトを出す。
    void PlayDashStartBurst();
    // 移動に追従するループエフェクトを停止する。
    void StopMovementEffects();
    // 指定したボーンを探し、見つかった場合にワールド位置を書き出す。
    bool TryGetJointWorldPosition(
        const std::string& jointName,
        Vector3& worldPosition) const;

    enum class PlayerAnimState {
        Idle,
        CombatIdle,
        Running,
        Dashing,
        Jumping,
        Attacking
    };

    std::unique_ptr<Camera> camera_;
    std::unique_ptr<DebugCameraController> debugCameraController_;
    std::unique_ptr<Object3d> floorObj_;
    std::unique_ptr<Object3d> ikTerrainObj_;
    Model* ikTerrainModel_ = nullptr;
    Vector3 ikTerrainPosition_ = { 0.0f, -5.0f, 0.0f };
    Vector3 ikTerrainScale_ = { 1.0f, 0.35f, 1.0f };
    static constexpr size_t kIkTestBlockCount = 7;
    std::array<std::unique_ptr<Object3d>, kIkTestBlockCount>
        ikTestBlockObjs_;
    std::array<Vector3, kIkTestBlockCount> ikTestBlockPositions_ = {
        Vector3 { -0.55f, -4.95f, 0.0f },
        Vector3 { 0.55f, -4.88f, 0.0f },
        Vector3 { -1.65f, -4.91f, 0.0f },
        Vector3 { 1.65f, -4.84f, 0.0f },
        Vector3 { -1.15f, -4.90f, 2.5f },
        Vector3 { 0.0f, -4.86f, 2.5f },
        Vector3 { 1.15f, -4.82f, 2.5f }
    };
    std::array<Vector3, kIkTestBlockCount> ikTestBlockScales_ = {
        Vector3 { 0.90f, 0.18f, 2.20f },
        Vector3 { 0.90f, 0.30f, 2.20f },
        Vector3 { 0.90f, 0.24f, 2.20f },
        Vector3 { 0.90f, 0.38f, 2.20f },
        Vector3 { 1.00f, 0.20f, 2.00f },
        Vector3 { 1.00f, 0.24f, 2.00f },
        Vector3 { 1.00f, 0.28f, 2.00f }
    };
    std::array<Vector3, kIkTestBlockCount> ikTestBlockRotations_ = {
        Vector3 { 0.0f, 0.0f, 0.0f },
        Vector3 { 0.0f, 0.0f, 0.0f },
        Vector3 { 0.0f, 0.0f, 0.0f },
        Vector3 { 0.0f, 0.0f, 0.0f },
        Vector3 { 0.0f, 0.0f, -0.10f },
        Vector3 { 0.12f, 0.0f, 0.08f },
        Vector3 { -0.10f, 0.0f, 0.12f }
    };
    std::unique_ptr<Object3d> katanaObj_;
    std::unique_ptr<Object3d> recoveryCubeObj_;
    Vector3 recoveryCubeBasePosition_ = { -6.0f, -3.5f, -4.0f };
    float recoveryCubeAnimationTime_ = 0.0f;
    EffectHandle recoveryEffectHandle_ = kInvalidEffectHandle;

    // Player (Robo)
    std::unique_ptr<AnimationActor> playerActor_;
    std::unique_ptr<AnimationActor> sneakWalkActor_;
    Animation runAnimation_;
    Animation dashAnimation_;
    Animation jumpAnimation_;
    Animation idleAnimation_;
    Animation combatIdleAnimation_;
    Animation attackAnimation_;
    Animation leftPunchAnimation_;
    Animation rocketUppercutAnimation_;
    Vector3 playerPos_ = { 0.0f, -5.0f, 0.0f };
    Vector3 playerRot_ = { 0.0f, 0.0f, 0.0f };
    float playerScale_ = 0.4f;
    float playerRotOffset_ = 0.0f;
    Vector3 katanaGripPosition_ = { -1.3f, 0.7f, -2.2f };
    Vector3 katanaOffset_ = { 0.0f, 0.0f, 0.0f };
    Vector3 katanaRotation_ = { std::numbers::pi_v<float>, 0.0f, 0.0f };
    float katanaScale_ = 0.65f;
    PlayerAnimState currentAnimState_ = PlayerAnimState::Idle;
    float idleVariationTimer_ = 0.0f;
    float combatIdleTimer_ = 0.0f;

    EffectHandle fieldDemoEffectHandle_ = kInvalidEffectHandle;
    EffectHandle cyberSingularityEffectHandle_ = kInvalidEffectHandle;
    EffectHandle cyberSingularityDebrisHandle_ = kInvalidEffectHandle;
    EffectHandle cyberSingularityJetsHandle_ = kInvalidEffectHandle;
    EffectHandle sandstormGolemEffectHandle_ = kInvalidEffectHandle;
    bool isSandGolemMode_ = false;
    EffectHandle bodySpeedLineEffectHandle_ = kInvalidEffectHandle;
    EffectHandle backflipTrailEffectHandle_ = kInvalidEffectHandle;
    EffectHandle groundLightningOuterHandle_ = kInvalidEffectHandle;
    EffectHandle groundLightningInnerHandle_ = kInvalidEffectHandle;
    EffectHandle groundLightningMotesHandle_ = kInvalidEffectHandle;
    bool showFieldDebug_ = false;
    bool showSkeletonDebug_ = true;
    bool enableFootIK_ = true;
    bool showFootIKDebug_ = true;
    float leftFootIkWeight_ = 0.0f;
    float rightFootIkWeight_ = 0.0f;
    float footSoleGroundMargin_ = 0.08f;

    // TPS Camera
    float cameraYaw_ = 0.0f;
    float cameraPitch_ = 0.2f;
    float cameraDistance_ = 18.0f;

    // Jump Control
    bool isJumping_ = false;
    float jumpVelocityUnitsPerSecond_ = 0.0f;
    static constexpr float kGravityUnitsPerSecondSquared = 54.0f;

    // Attack Control
    float attackTimer_ = 0.0f;
    int comboStep_ = 0;
    int queuedComboAttacks_ = 0;
    std::string lastAnimationEventName_;

    std::array<PostEffectType, 34> postEffectTypes_ = {
        PostEffectType::Copy,
        PostEffectType::GrayScale,
        PostEffectType::Vignette,
        PostEffectType::DepthOfField,
        PostEffectType::MotionBlur,
        PostEffectType::ChromaticAberration,
        PostEffectType::LensDistortion,
        PostEffectType::FilmGrain,
        PostEffectType::LensDirt,
        PostEffectType::CameraShake,
        PostEffectType::BokehShape,
        PostEffectType::Fisheye,
        PostEffectType::Pixelate,
        PostEffectType::ColorAdjust,
        PostEffectType::smoothing,
        PostEffectType::GaussianFilter,
        PostEffectType::LuminanceBasedOutline,
        PostEffectType::DepthOutline,
        PostEffectType::RadialBlur,
        PostEffectType::Dissolve,
        PostEffectType::Random,
        PostEffectType::Bloom,
        PostEffectType::LensFlare,
        PostEffectType::Glare,
        PostEffectType::LightShafts,
        PostEffectType::VolumetricLight,
        PostEffectType::AnamorphicFlare,
        PostEffectType::Halo,
        PostEffectType::LightStreak,
        PostEffectType::NeonGlow,
        PostEffectType::GhostImage,
        PostEffectType::Outline,
        PostEffectType::Fog,
        PostEffectType::FocusLine,
    };
    std::size_t selectedPostEffectIndex_ = 0;

    // Snow Effect
    EffectHandle snowEffectHandle_ = kInvalidEffectHandle;

    // Hand Joint Flame Tracking Effects
    EffectHandle leftHandFlameHandle_ = kInvalidEffectHandle;
    EffectHandle rightHandFlameHandle_ = kInvalidEffectHandle;

    // Hinokami Kagura Effects
    EffectHandle hinokamiFlameHandle_ = kInvalidEffectHandle;
    EffectHandle hinokamiEmbersHandle_ = kInvalidEffectHandle;

    // Delayed 6-Direction Firework Launch & Double Burst Sequence Triggers
    float sixTrailTimer_ = 0.0f;
    bool isSixTrailPending_ = false;
    float sixBlastTimer_ = 0.0f;
    bool isSixBlastPending_ = false;

    // 二重破裂（2段階目爆発）用タイマー
    float secondBlastTimer_ = 0.0f;
    bool isSecondBlastPending_ = false;

    // 一連の演出全体のクールダウン・連打防止ガード
    float coolDownTimer_ = 0.0f;
    bool isSequenceActive_ = false;

    Vector3 sixSeqCenterPos_{};
    std::vector<std::unique_ptr<Text>> jointNameTexts_;
    // 現在のモデルからボーン名を集め、開発画面で選択できるようにする。
    void UpdateBoneNames();
};

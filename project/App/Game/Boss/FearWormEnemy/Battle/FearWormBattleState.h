#pragma once

#include "../State/FearWormState.h"
#include "Engine/Math/MathStruct.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

class Model;
class Object3d;
class DxrRenderer;

// 戦闘中の移動・攻撃・ビームの実処理を担当する。
class FearWormBattleState final : public FearWormState {
public:
    FearWormBattleState();
    ~FearWormBattleState() override;
    void Initialize(Model* bulletModel);
    void Reset(FearWormEnemy& boss);
    void PrepareEntryMovement(FearWormEnemy& boss, float orbitAngle);
    void StopAttacksForDeath(FearWormEnemy& boss);
    bool IsMadModeActive() const { return isMadModeActive_; }
    bool IsBeamHittingPlayer() const { return isBeamHittingPlayer_; }
    void Update(FearWormEnemy& boss) override;

private:
    friend class FearWormEnemy;
    friend bool RunBeamStateTests();
    friend bool RunFearWormMovementTests();
    friend bool RunFearWormEntryTests();
    friend bool RunFearWormStateTests();

    // 戦闘専用の設定値。
    static constexpr float kInitialHeadChargeCooldown = 1.80f;
    static constexpr Vector3 kDefaultHeadAimDirection = { 0.0f, 0.0f, -1.0f };
    static constexpr float kMissileAimWaitDuration = 2.0f;   // 整列完了（前準備）を待つ時間（秒）
    static constexpr float kMissileAttackInterval = 18.0f;  // ミサイル攻撃の間隔（秒）
    static constexpr float kMissileFireInterval = 0.45f;    // ミサイルの発射間隔（秒）
    static constexpr float kOrbitAngularSpeed = 1.15f;
    static constexpr float kOrbitRadiusX = 16.0f;
    static constexpr float kOrbitRadiusY = 7.0f;
    static constexpr float kOrbitHeight = 7.0f;
    static constexpr float kOrbitLateralSwayFrequency = 2.0f;
    static constexpr float kOrbitLateralSwayAmplitude = 2.0f;
    static constexpr float kOrbitDepthSwayFrequency = 0.80f;
    static constexpr float kOrbitDepthSwayAmplitude = 3.0f;
    static constexpr float kCoilAngleRate = 2.35f;
    static constexpr float kCoilBaseRadius = 7.0f;
    static constexpr float kCoilHeight = 30.0f;
    static constexpr float kCoilSegmentAngleRadians = 0.65f;
    static constexpr float kCoilSegmentHeightRate = 0.46f;
    static constexpr float kCoilFormationDurationSeconds = 1.1f;
    static constexpr float kCoilAttackDurationSeconds = 2.4f;
    static constexpr float kCoilReleaseDurationSeconds = 0.8f;
    static constexpr float kWeaveAngleRate = 1.35f;
    static constexpr float kWeaveWidth = 13.0f;
    static constexpr float kWeaveHeight = 7.0f;
    static constexpr float kWeaveVerticalFrequency = 2.0f;
    static constexpr float kWeaveVerticalAmplitude = 9.0f;
    static constexpr float kWeaveDepthAmplitude = 2.5f;
    static constexpr float kDriftHorizontalFrequency = 0.65f;
    static constexpr float kDriftHorizontalAmplitude = 8.0f;
    static constexpr float kDriftHeight = 8.0f;
    static constexpr float kDriftVerticalFrequency = 0.85f;
    static constexpr float kDriftVerticalAmplitude = 4.5f;
    static constexpr float kDriftDepthFrequency = 0.50f;
    static constexpr float kDriftDepthAmplitude = 2.0f;
    static constexpr float kAttackRange = 240.0f;
    static constexpr float kHeadChargeDuration = 0.85f;
    static constexpr float kHeadChargeShotInterval = 0.22f;
    static constexpr float kHeadChargeCooldown = 3.10f;
    static constexpr float kHeadChargeEffectInterval = 0.14f;
    static constexpr float kChargedBulletSpeed = 1.55f;
    static constexpr float kChargedBulletScale = 0.88f;
    static constexpr float kChargedBulletCollisionRadius = 2.75f;
    static constexpr int32_t kHeadChargeShotMax = 3;
    static constexpr int32_t kChargedBulletDamage = 3;
    static constexpr float kHeadAimFollowRate = 0.18f;
    static constexpr Vector4 kChargedBulletColor = { 1.0f, 0.353f, 0.239f, 1.0f };
    static constexpr float kChargedBulletLifeTime = 20.0f;
    static constexpr float kHeadMuzzleOffset = 4.2f;
    static constexpr Vector4 kChargingHeadColor = { 1.0f, 0.48f, 0.08f, 1.0f };
    static constexpr Vector4 kFiringHeadColor = { 1.0f, 0.08f, 0.02f, 1.0f };
    static constexpr float kChargePulseAdd = 0.12f;
    static constexpr float kChargePulseFrequency = 24.0f;
    static constexpr float kChargePulseAmplitude = 0.08f;
    static constexpr float kMissilePostFireCooldown = 1.8f; // 撃ち終わった後の隙（秒）
    static constexpr float kPi = 3.1415926535f;
    static constexpr float kBeamUvScrollSpeed = -6.5f; // より高速に流す
    static constexpr float kBeamRotateSpeedRadiansPerSecond = 45.0f; // 高速自転（うねりの表現を強化）
    static constexpr int kBeamDamage = 1;
    static constexpr float kCoilBarrageFireIntervalSeconds = 0.6f;
    static constexpr int32_t kCoilBarrageBulletCount = 6;
    static constexpr float kCoilBarrageSegmentAngleRadians = 0.17f;
    static constexpr float kCoilBarrageAngularSpeedRadiansPerSecond = 0.35f;
    static constexpr float kCoilBarrageSpreadRatio = 0.35f;
    static constexpr float kNormalSpreadAngleRadians = 0.14f;
    static constexpr int32_t kNormalBurstShotCount = 3;
    static constexpr float kNormalBurstShotIntervalSeconds = 0.12f;



    enum class FearWormMovementType { Orbit, Coil, Weave, Drift, Line, Spiral };

    struct FearWormMovementContext {
        Vector3 playerPosition;
        float orbitAngle;
        float patternTime;
        float forwardOffset;
        float madModeTime;
        float spiralAngle;
    };

    using MovementPattern = FearWormMovementType;
    enum class BeamState { Wait, Charge, Fire, FadeOut };
    enum class NormalAttackType { Single, Spread, FixedBurst };

    // 激昂中の攻撃と移動を更新し、通常時とは異なる圧力をかける。
    void UpdateMadMode(FearWormEnemy& boss, float movementSpeedRate);
    // 部位からミサイルを撃つ順番と発射タイミングを管理する。
    void UpdateMissileAttack(FearWormEnemy& boss);
    // プレイヤーを基準に移動先を決め、速度倍率を反映して頭を動かす。
    void UpdateBattleMovement(FearWormEnemy& boss, const Vector3& playerPosition, float movementSpeedRate);
    // 戦闘中の攻撃タイマーを進め、選択された攻撃の処理へつなげる。
    void UpdateBattleAttack(FearWormEnemy& boss);
    // プレイヤーの周囲を回り込む軌道上の目標位置を求める。
    static Vector3 CalculateOrbitTarget(const FearWormMovementContext& context);
    // 巻き付くように周回する目標位置を求める。
    static Vector3 CalculateCoilTarget(const FearWormMovementContext& context);
    // 左右や上下に揺れながら進む目標位置を求める。
    static Vector3 CalculateWeaveTarget(const FearWormMovementContext& context);
    // ゆっくり漂う移動パターンの目標位置を求める。
    static Vector3 CalculateDriftTarget(const FearWormMovementContext& context);
    // 渦を描く移動パターンの目標位置を求める。
    static Vector3 CalculateSpiralTarget(const FearWormMovementContext& context);
    // 直線移動パターンの目標位置を求める。
    static Vector3 CalculateLineTarget(const FearWormMovementContext& context);
    // 移動の種類に対応する軌道計算を選ぶ。
    static Vector3 CalculateFearWormTarget(FearWormMovementType type, const FearWormMovementContext& context);
    // 次に使用する移動パターンを選ぶ。
    static FearWormMovementType NextFearWormMovement(FearWormMovementType type);
    // 戦闘用の軌道と経過時間をもとに、現在の追従先を計算する。
    Vector3 BattleTarget(FearWormEnemy& boss, const Vector3& playerPosition, float rate);
    // 現在の移動状態に応じた目標位置を取得する。
    Vector3 MovementTargetPosition(const FearWormEnemy& boss, const Vector3& playerPosition) const;
    // 経過時間を確認し、移動パターンの継続と切替を行う。
    void UpdateMovementPattern(FearWormEnemy& boss, float movementSpeedRate);
    // 移動パターンを変更し、そのパターンで使う経過時間をリセットする。
    void ChangeMovementPattern(FearWormEnemy& boss, MovementPattern pattern);
    // 射撃タイミングを管理し、通常弾や特殊攻撃を開始する。
    void Attack(FearWormEnemy& boss);
    // 頭の溜め攻撃を、準備・発射・終了の順に進める。
    void UpdateHeadChargeAttack(FearWormEnemy& boss, float attackSpeedRate);
    // 頭の溜め攻撃に必要なタイマーと照準状態を準備する。
    void StartHeadChargeAttack(FearWormEnemy& boss);
    // 溜め攻撃の状態を解除し、次の攻撃へ戻れるようにする。
    void FinishHeadChargeAttack(FearWormEnemy& boss);
    // プレイヤーの位置へ頭の狙いを合わせる。
    void UpdateHeadAimDirection(FearWormEnemy& boss);
    // 指定した部位の位置から、通常射撃用の弾を生成する。
    void FireBullet(FearWormEnemy& boss, const Vector3& position);
    // 単発・扇状弾・固定方向の連射を順番に開始する。
    void FireNormalAttack(FearWormEnemy& boss, int32_t segmentIndex);
    void FireSpreadBullets(FearWormEnemy& boss, const Vector3& position, const Vector3& direction);
    void UpdateNormalBurst(FearWormEnemy& boss);
    void CancelNormalBurst();
    // 溜め終わった頭から強化された弾を発射する。
    void FireChargedBullet(FearWormEnemy& boss);
    // 頭の向きと大きさを考慮して、弾を出す位置を求める。
    Vector3 HeadMuzzlePosition(const FearWormEnemy& boss) const;
    // 頭が照準方向を向くように描画用の回転角を計算する。
    Vector3 HeadLookRotation(const FearWormEnemy& boss) const;
    // 指定した部位から追尾ミサイルを一発生成する。
    void FireSingleMissile(FearWormEnemy& boss, size_t segmentIndex);
    // 任意軸まわりの回転行列を生成する関数 (ロドリゲスの回転公式)
    static Matrix4x4 MakeRotateAxisAngleMatrix(const Vector3& axis, float angle);
    // 発射角度を回転させながら弾を出し、渦状の弾幕を作る。
    void UpdateSpiralBarrage(FearWormEnemy& boss);
    // 指定された位置と方向を使って弾を生成する。
    void FireDirectionalBullet(FearWormEnemy& boss, const Vector3& position, const Vector3& direction,
        bool shouldPlayMuzzleEffect = true);
    // ビーム本体と溜め演出を描画するリソースを準備する。
    void InitializeBeam(FearWormEnemy& boss);
    // 頭の位置と狙いにビームを追従させ、ビーム状態を更新する。
    void UpdateBeamAttack(FearWormEnemy& boss);
    // 現在のビーム状態に必要な溜め演出や光線を描画する。
    void DrawBeam(FearWormEnemy& boss);
    // 発射中の光線とプレイヤーの距離を調べ、命中時のダメージを処理する。
    void CheckBeamCollision(FearWormEnemy& boss);
    // 待機・溜め・発射・消失のうち、現在のビーム状態を進める。
    void UpdateBeamState(FearWormEnemy& boss, float deltaTime);
    // 次のビームを開始できる条件が整うまで待つ。
    void UpdateBeamWait(FearWormEnemy& boss, float);
    // 溜め時間と見た目を更新し、準備が終わったら発射へ移る。
    void UpdateBeamCharge(FearWormEnemy& boss, float dt);
    // 発射時間を進め、時間切れでビームを消す状態へ移る。
    void UpdateBeamFire(FearWormEnemy& boss, float dt);
    // 光線を徐々に消し、次の発射までの待機状態へ戻す。
    void UpdateBeamFadeOut(FearWormEnemy& boss, float dt);
    // ビーム状態を切り替え、各状態のタイマーや表示を設定する。
    void ChangeBeamState(FearWormEnemy& boss, BeamState state);
    // とぐろ弾幕攻撃
    void UpdateCoilBarrage(FearWormEnemy& boss);
    float CoilFormationBlend() const;
    bool IsCoilFiring() const;
    static Vector3 CalculateCoilSegmentPosition(const Vector3& headPosition, float orbitAngle,
        int32_t segmentOrder, float segmentSpacing);
    // 共通の胴体追従へ、攻撃用の隊形を合成する。
    float UpdateFormationBlend();
    Vector3 CalculateFormationPosition(const FearWormEnemy& boss, int32_t segmentOrder,
        const Vector3& followPosition, float formationBlend) const;
    // 共通の見た目を基準に、攻撃による色・拡縮・照準を反映する。
    Vector4 HeadAttackColor(const Vector4& baseColor) const;
    float ApplyHeadChargePulse(float basePulse) const;
    bool ShouldAimHead(const FearWormEnemy& boss) const;
    void SubmitRaytracing(FearWormEnemy& boss, DxrRenderer& renderer);

    // 各ボスの戦闘状態と攻撃リソース。
    Model* bulletModel_ = nullptr;
    float bulletSpeed_ = 1.05f;

    // 戦闘移動と登場時の移動準備で共有する状態。
    float orbitAngle_ = 0.0f;
    float movementPatternTimer_ = 0.0f;
    float movementPatternDuration_ = 3.40f;

    // 通常射撃と頭部チャージ。
    int32_t fireTimer_ = 0;
    int32_t fireInterval_ = 22;
    int32_t fireSegmentIndex_ = 0;
    NormalAttackType normalAttackType_ = NormalAttackType::Single;
    int32_t normalBurstSegmentIndex_ = 0;
    int32_t normalBurstShotsRemaining_ = 0;
    float normalBurstShotTimerSeconds_ = 0.0f;
    Vector3 normalBurstDirection_ = { 0.0f, 0.0f, -1.0f };
    int32_t headChargeShotCount_ = 0;
    float headChargeTimer_ = 0.0f;
    float headChargeShotTimer_ = 0.0f;
    float headChargeCooldownTimer_ = 0.0f;
    float headChargeEffectTimer_ = 0.0f;
    Vector3 headAimDirection_ = { 0.0f, 0.0f, 0.0f };
    bool isHeadChargeActive_ = false;
    bool isHeadChargeFiring_ = false;
    MovementPattern movementPattern_ = MovementPattern::Orbit;

    // ミサイルの準備と連続発射。
    float missileAttackTimer_ = 0.0f;
    bool isMissileAttackActive_ = false;
    float missilePhaseTimer_ = 0.0f;
    bool hasFiredMissiles_ = false;
    bool isFiringMissilesSequence_ = false;
    float missileFireTimer_ = 0.0f;
    int32_t missileShotCount_ = 0;
    std::vector<size_t> missileTargetIndices_;

    // 隊形補間・渦巻き弾幕・激昂。
    float lineTransitionTimer_ = 0.0f;
    float coilBarrageNextFireSeconds_ = 0.0f;
    float spiralAngle_ = 0.0f;
    float barrageAngle_ = 0.0f;
    int32_t barrageFireTimer_ = 0;
    float madModeTimer_ = 0.0f;
    bool isMadModeActive_ = false;
    bool isMadModeFinished_ = false;

    // ビームの描画リソースと攻撃状態。
    std::unique_ptr<Object3d> beamPlane_;
    BeamState beamState_ = BeamState::Wait;
    float beamTimer_ = 0.0f;
    float beamUVScrollOffset_ = 0.0f;
    float beamCurrentLength_ = 0.0f;
    float beamCurrentWidth_ = 0.0f;
    float beamRotateTheta_ = 0.0f;
    bool isBeamHittingPlayer_ = false;
    float beamEffectTimer_ = 0.0f;
    Vector3 beamMuzzlePosition_ {};
    int32_t beamHitEffectFrameCount_ = 0;
};

#pragma once

#include "../State/FearWormState.h"
#include "OrbitShot/FearWormOrbitShotState.h"
#include "WeaveSpread/FearWormWeaveSpreadState.h"
#include "DriftBurst/FearWormDriftBurstState.h"
#include "CoilBarrage/FearWormCoilBarrageState.h"
#include "HeadCharge/FearWormHeadChargeState.h"
#include "LineMissile/FearWormLineMissileState.h"
#include "SpiralBarrage/FearWormSpiralBarrageState.h"
#include "Beam/FearWormBeamState.h"
#include "Engine/Math/MathStruct.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

class Model;
class Object3d;
class DxrRenderer;

// 移動と攻撃をまとめた行動の選択、共通の座標計算と描画資源を管理する。
class FearWormBattleState final : public FearWormState {
public:
    FearWormBattleState();
    ~FearWormBattleState() override;
    void Initialize(Model* bulletModel);
    void Reset(FearWormEnemy& boss);
    void PrepareEntryMovement(FearWormEnemy& boss, float orbitAngle);
    void StopAttacksForDeath(FearWormEnemy& boss);
    bool IsMadModeActive() const { return currentAction_ != nullptr && actionType_ == ActionType::SpiralBarrage; }
    bool IsBeamHittingPlayer() const { return beamActionState_.IsHittingPlayer(); }
    void Enter(FearWormEnemy& boss) override;
    void Update(FearWormEnemy& boss) override;
    void Exit(FearWormEnemy& boss) override;

private:
    friend class FearWormEnemy;
    friend class FearWormOrbitShotState;
    friend class FearWormWeaveSpreadState;
    friend class FearWormDriftBurstState;
    friend class FearWormCoilBarrageState;
    friend class FearWormHeadChargeState;
    friend class FearWormLineMissileState;
    friend class FearWormSpiralBarrageState;
    friend class FearWormBeamState;
    friend bool RunFearWormBattleActionTests();
    friend bool RunBeamStateTests();
    friend bool RunFearWormMovementTests();
    friend bool RunFearWormEntryTests();
    friend bool RunFearWormStateTests();

    // 戦闘専用の設定値。
    static constexpr Vector3 kDefaultHeadAimDirection = { 0.0f, 0.0f, -1.0f };
    static constexpr float kMissileAttackInterval = 18.0f;  // ミサイル攻撃の間隔（秒）
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
    static constexpr float kHeadMuzzleOffset = 4.2f;
    static constexpr float kPi = 3.1415926535f;

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
    enum class ActionType { OrbitShot, WeaveSpread, DriftBurst, CoilBarrage, HeadCharge, LineMissile, SpiralBarrage, Beam };

    // プレイヤーを基準に移動先を決め、速度倍率を反映して頭を動かす。
    void UpdateBattleMovement(FearWormEnemy& boss, const Vector3& playerPosition, float movementSpeedRate,
        float actionElapsedSeconds = 0.0f, float spiralAngleRadians = 0.0f);
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
    // 戦闘用の軌道と経過時間をもとに、現在の追従先を計算する。
    Vector3 BattleTarget(FearWormEnemy& boss, const Vector3& playerPosition,
        float actionElapsedSeconds = 0.0f, float spiralAngleRadians = 0.0f);
    // 現在の移動状態に応じた目標位置を取得する。
    Vector3 MovementTargetPosition(const FearWormEnemy& boss, const Vector3& playerPosition,
        float actionElapsedSeconds = 0.0f, float spiralAngleRadians = 0.0f) const;
    // 指定した部位の位置から、通常射撃用の弾を生成する。
    void FireBullet(FearWormEnemy& boss, const Vector3& position);
    // 頭の向きと大きさを考慮して、弾を出す位置を求める。
    Vector3 HeadMuzzlePosition(const FearWormEnemy& boss) const;
    // 頭が照準方向を向くように描画用の回転角を計算する。
    Vector3 HeadLookRotation(const FearWormEnemy& boss) const;
    // 指定された位置と方向を使って弾を生成する。
    void FireDirectionalBullet(FearWormEnemy& boss, const Vector3& position, const Vector3& direction,
        bool shouldPlayMuzzleEffect = true);
    // ビーム本体と溜め演出を描画するリソースを準備する。
    void InitializeBeam(FearWormEnemy& boss);
    // 現在のビーム状態に必要な溜め演出や光線を描画する。
    void DrawBeam(FearWormEnemy& boss);
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

    void ChangeAction(ActionType type, FearWormEnemy& boss);
    void FinishAction(FearWormEnemy& boss);
    Model* GetBulletModel() const { return bulletModel_; }
    bool CanFire(const FearWormEnemy& boss) const;
    int32_t SelectFiringSegment(FearWormEnemy& boss, int32_t& fireTimer);

    // 行動は各ボスが所有し、切り替え時の動的確保は行わない。
    FearWormOrbitShotState orbitShotState_;
    FearWormWeaveSpreadState weaveSpreadState_;
    FearWormDriftBurstState driftBurstState_;
    FearWormCoilBarrageState coilBarrageState_;
    FearWormHeadChargeState headChargeState_;
    FearWormLineMissileState lineMissileState_;
    FearWormSpiralBarrageState spiralBarrageState_;
    FearWormBeamState beamActionState_;
    FearWormBattleAction* currentAction_ = nullptr;
    ActionType actionType_ = ActionType::OrbitShot;

    // 各ボスの戦闘状態と攻撃リソース。
    Model* bulletModel_ = nullptr;
    float bulletSpeed_ = 1.05f;

    // 戦闘移動と登場時の移動準備で共有する状態。
    float orbitAngle_ = 0.0f;
    float movementPatternTimer_ = 0.0f;
    float movementPatternDuration_ = 3.40f;

    // 共通の射撃設定と描画用の照準・移動情報。
    int32_t fireInterval_ = 22;
    int32_t fireSegmentIndex_ = 0;
    Vector3 headAimDirection_ = { 0.0f, 0.0f, 0.0f };
    MovementPattern movementPattern_ = MovementPattern::Orbit;

    // 通常行動の終了時にミサイルへ移るための待機時間。
    float missileAttackTimer_ = 0.0f;

    // 隊形補間と、激昂を一度だけ実行するための記録。
    float lineTransitionTimer_ = 0.0f;
    bool hasTriggeredMadMode_ = false;

};

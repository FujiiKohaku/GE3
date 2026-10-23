#pragma once

#include <numbers>
#include "../State/FearWormState.h"
#include "Movement/FearWormMovement.h"
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
    void SetStartOrbitAngleRadians(float orbitAngleRadians);
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
    static constexpr float kAttackRange = 240.0f;
    static constexpr float kHeadMuzzleOffset = 4.2f;
    static constexpr float kPi = std::numbers::pi_v<float>;

    enum class FearWormMovementType { Orbit, Coil, Weave, Drift, Line, Spiral };

    using FearWormMovementContext = ::FearWormMovementContext;

    using MovementPattern = FearWormMovementType;
    enum class ActionType { OrbitShot, WeaveSpread, DriftBurst, CoilBarrage, HeadCharge, LineMissile, SpiralBarrage, Beam };

    // 攻撃側で計算した目標へ、共通の追従処理を適用する。
    void UpdateBattleMovement(FearWormEnemy& boss, const Vector3& target, float movementSpeedRate);
    // 座標計算の実装は各攻撃と共通軌道へ委譲する。
    static Vector3 CalculateFearWormTarget(FearWormMovementType type, const FearWormMovementContext& context,
        float actionElapsedSeconds = 0.0f, float spiralAngleRadians = 0.0f);
    // プレイヤー位置をカメラの視線で補正し、攻撃の座標計算に渡す。
    FearWormMovementContext CreateMovementContext(const FearWormEnemy& boss, const Vector3& playerPosition) const;
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
    // 共通の胴体追従へ、攻撃用の隊形を合成する。
    float GetFormationBlend() const;
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
    int32_t SelectFiringSegment(FearWormEnemy& boss);

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

    // 戦闘移動の周回角と、通常行動の継続時間設定。
    float orbitAngle_ = 0.0f;
    float movementPatternDuration_ = 3.40f;

    // 共通の射撃設定と描画用の照準・移動情報。
    static constexpr float kFireIntervalSeconds = 22.0f / 60.0f;
    int32_t fireSegmentIndex_ = 0;
    Vector3 headAimDirection_ = { 0.0f, 0.0f, 0.0f };
    MovementPattern movementPattern_ = MovementPattern::Orbit;

    // 通常行動の終了時にミサイルへ移るための待機時間。
    float missileAttackTimer_ = 0.0f;

    // 激昂を一度だけ実行するための記録。
    bool hasTriggeredMadMode_ = false;

};

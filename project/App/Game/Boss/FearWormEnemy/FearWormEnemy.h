#pragma once

#include "App/Game/Boss/Common/BaseBoss.h"
class Model;
class Object3d;
class Player;

class FearWormEnemy : public BaseBoss {
public:
    enum class BossState {
        Wait,    // プレイヤーを待つ待機状態
        Entry,   // 登場演出状態 (Lerpによるスライドイン)
        Battle,  // 通常戦闘状態 (行動切り替えと攻撃)
        Dead     // 撃破演出状態
    };

    FearWormEnemy();

    void Initialize(
        Model* model,
        Model* bulletModel,
        Player* player);

    void Update() override;

    // 生存中の各部位とボスが発射した弾を描画する。
    void Draw() override;
    void SubmitRaytracing(DxrRenderer& renderer) override;
    void DrawShadow(ShadowMapRenderer& renderer) override;

    // 撃破演出が完了してシーン遷移可能かを返す。
    bool IsDeathSequenceFinished() const override;

    // 頭部をボスの代表位置として返す。
    Vector3 GetPosition() const override;

    // ボスの基準位置を設定し、全部位と移動状態をリセットする。
    void SetPosition(const Vector3& position) override;

    // 生存している頭部と胴体の当たり判定情報を取得する。
    void GetCollisionParts(std::vector<EnemyCollisionPart>& parts) const override;

    // 指定部位が現在ダメージを受けられる状態かを返す。
    bool IsCollisionPartDamageable(int32_t partIndex) const override;

    // 指定部位へダメージを与え、部位破壊または撃破を処理する。
    void ApplyDamageToPart(int32_t partIndex, float damage) override;

    // 無効な攻撃を受けた頭部のガード演出を再生する。
    void OnCollisionPartGuarded(int32_t partIndex, const Vector3& position) override;

    // 現在の状態を取得する
    BossState GetBossState() const;
    bool IsMadModeActive() const override { return isMadModeActive_; }
    bool IsBeamHittingPlayer() const override { return isBeamHittingPlayer_; }

    // HP割合のGetter
    float GetHeadHpFraction() const override;
    float GetBodyHpFraction() const override;

private:
    friend bool RunBeamStateTests();
    friend bool RunFearWormMovementTests();

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

    struct Segment {
        std::unique_ptr<Object3d> object;
        Vector3 position = { 0.0f, 0.0f, 0.0f };
        Vector3 scale = { 1.0f, 1.0f, 1.0f };
        float hp = 1.0f;
        float radius = 1.0f;
        float hitFlashTimer = 0.0f;
        bool isHead = false;
        bool isAlive = true;
    };

    // Updateの入口と状態の選択。
    void UpdateState();
    void ChangeState(BossState state);
    void OnDeath() override;

    // 待機 → 登場 → 戦闘 → 死亡。
    void EnterEntry();
    void EnterDead();
    void UpdateWait();
    void UpdateEntry();
    void UpdateBattle();
    void UpdateDead();
    void UpdateMadMode(float movementSpeedRate);
    void UpdateMissileAttack();
    void UpdateBattleMovement(const Vector3& playerPosition, float movementSpeedRate);
    void UpdateBattleAttack();

    // 目的地、移動パターン、胴体の追従。
    static Vector3 CalculateOrbitTarget(const FearWormMovementContext& context);
    static Vector3 CalculateCoilTarget(const FearWormMovementContext& context);
    static Vector3 CalculateWeaveTarget(const FearWormMovementContext& context);
    static Vector3 CalculateDriftTarget(const FearWormMovementContext& context);
    static Vector3 CalculateLineTarget(const FearWormMovementContext& context);
    static Vector3 CalculateSpiralTarget(const FearWormMovementContext& context);
    static Vector3 CalculateFearWormTarget(FearWormMovementType type, const FearWormMovementContext& context);
    static FearWormMovementType NextFearWormMovement(FearWormMovementType type);
    static Matrix4x4 MakeRotateAxisAngleMatrix(const Vector3& axis, float angle);
    Vector3 FallbackTarget();
    Vector3 EntryTarget(const Vector3& playerPosition);
    Vector3 BattleTarget(const Vector3& playerPosition, float rate);
    Vector3 EntryStartPosition(const Vector3& playerPosition) const;
    Vector3 MovementTargetPosition(const Vector3& playerPosition) const;
    void ChangeMovementPattern(MovementPattern pattern);
    void UpdateMovementPattern(float movementSpeedRate);
    void UpdateSegments();
    float MovementSpeedRate() const;

    // 通常弾、チャージ弾、ミサイル、弾幕。
    void Attack() override;
    void FireBullet(const Vector3& position);
    void FireSingleMissile(size_t segmentIndex);
    void UpdateHeadChargeAttack(float attackSpeedRate);
    void StartHeadChargeAttack();
    void FinishHeadChargeAttack();
    void UpdateHeadAimDirection();
    void FireChargedBullet();
    Vector3 HeadMuzzlePosition() const;
    Vector3 HeadLookRotation() const;
    void UpdateSpiralBarrage();
    void FireDirectionalBullet(const Vector3& position, const Vector3& direction);

    // 部位の生成、描画、当たり判定、HP。
    void InitializeSegments(Model* model);
    void UpdateSegmentObjects();
    void PlayBodyBreakEffect(const Vector3& position);
    void PlayHeadGuardEffect(const Vector3& position);
    void PlayHeadVulnerableEffect(const Vector3& position);
    float HealthRate() const;
    bool HasAliveBodyParts() const;
    bool IsValidSegmentIndex(int32_t partIndex) const;

    // 待機 → チャージ → 発射 → 終了。
    void InitializeBeam();
    void UpdateBeamAttack();
    void ChangeBeamState(BeamState state);
    void UpdateBeamState(float deltaTime);
    void UpdateBeamWait(float deltaTime);
    void UpdateBeamCharge(float deltaTime);
    void UpdateBeamFire(float deltaTime);
    void UpdateBeamFadeOut(float deltaTime);
    void DrawBeam();
    void CheckBeamCollision();

    BossState state_ = BossState::Wait;
    Player* player_ = nullptr;
    Model* bulletModel_ = nullptr;

    std::vector<Segment> segments_;

    Vector3 startPosition_ = { 0.0f, 0.0f, 0.0f };

    float moveTime_ = 0.0f;
    float segmentSpacing_ = 5.0f;
    float bulletSpeed_ = 1.05f;
    float activationLeadDistance_ = 160.0f;
    float parallelForwardOffset_ = 52.0f;
    float enterTimer_ = 0.0f;
    float enterDuration_ = 1.20f;
    float orbitAngle_ = 0.0f;
    float movementPatternTimer_ = 0.0f;
    float movementPatternDuration_ = 3.40f;

    int32_t fireTimer_ = 0;
    int32_t fireInterval_ = 22;
    int32_t fireSegmentIndex_ = 0;
    int32_t headChargeShotCount_ = 0;

    float headChargeTimer_ = 0.0f;
    float headChargeShotTimer_ = 0.0f;
    float headChargeCooldownTimer_ = 0.0f;
    float headChargeEffectTimer_ = 0.0f;
    Vector3 headAimDirection_ = { 0.0f, 0.0f, 0.0f };

    bool vulnerableEffectPlayed_ = false;
    bool isHeadChargeActive_ = false;
    bool isHeadChargeFiring_ = false;
    bool isDeathSequenceFinished_ = false;
    float deathSequenceTimer_ = 0.0f;
    int32_t nextDeathSegmentIndex_ = -1;
    MovementPattern movementPattern_ = MovementPattern::Orbit;

    // 追尾ミサイル攻撃用のタイマー・フラグ
    float missileAttackTimer_ = 0.0f;
    bool isMissileAttackActive_ = false;
    float missilePhaseTimer_ = 0.0f;
    bool hasFiredMissiles_ = false;

    // 時間差ミサイル発射用
    bool isFiringMissilesSequence_ = false;
    float missileFireTimer_ = 0.0f;
    int32_t missileShotCount_ = 0;
    std::vector<size_t> missileTargetIndices_;

    // Lineパターンへの遷移補間用
    float lineTransitionTimer_ = 0.0f;

    // 渦巻き・弾幕パラメータ
    float spiralAngle_ = 0.0f;
    float barrageAngle_ = 0.0f;
    int32_t barrageFireTimer_ = 0;

    // 発狂モード制限時間用
    float madModeTimer_ = 0.0f;
    bool isMadModeActive_ = false;
    bool isMadModeFinished_ = false;

    // 死亡演出（落下）用のパラメータ
    Vector3 deathVelocity_ = { 0.0f, 0.0f, 0.0f };      // 落下速度ベクトル
    Vector3 deathRotation_ = { 0.0f, 0.0f, 0.0f };      // 落下中の回転速度ベクトル
    Vector3 currentDeathRotation_ = { 0.0f, 0.0f, 0.0f };// 現在の回転角

    // ビーム攻撃用メンバー変数
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

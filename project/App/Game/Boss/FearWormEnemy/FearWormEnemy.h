// 頭と胴体が連なるボス。水面からの登場、移動、各種攻撃、部位破壊と死亡演出を扱う。
#pragma once

#include "App/Game/Boss/Common/BaseBoss.h"
#include "Wait/FearWormWaitState.h"
#include "Entry/FearWormEntryState.h"
#include "Battle/FearWormBattleState.h"
#include "Dead/FearWormDeadState.h"
class Model;
class Object3d;
class Player;

class FearWormEnemy : public BaseBoss {
public:

    // ボスの大まかな状態を表す列挙型。各状態はFearWormState派生クラスで処理される。
    enum class BossState {
        Wait,    // プレイヤーを待つ待機状態
        Entry,   // 水面の予兆から順に飛び出す登場演出
        Battle,  // 通常戦闘状態 (行動切り替えと攻撃)
        Dead     // 撃破演出状態
    };

    FearWormEnemy();

    // 敵としての共通状態、連結部位とビームなどの攻撃用リソースを準備する。
    void Initialize(
        Model* model,
        Model* bulletModel,
        Player* player);

    // ボスの状態を進め、連結部位と描画オブジェクトを現在の位置へ追従させる。
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
    bool HasBreachedSurface() const { return entryState_.HasBreachedSurface(); }
    void SetEntryWaterHeight(float waterHeight) { entryState_.SetWaterHeight(waterHeight); }
    bool IsMadModeActive() const override { return isMadModeActive_; }
    bool IsBeamHittingPlayer() const override { return isBeamHittingPlayer_; }

    // HP割合のGetter
    float GetHeadHpFraction() const override;
    // 胴体の残りHPをまとめて割合で返し、ボスのHP表示に使う。
    float GetBodyHpFraction() const override;

private:
    friend class FearWormWaitState;
    friend class FearWormEntryState;
    friend class FearWormJumOutState;
    friend class FearWormWaitForBodyState;
    friend class FearWormBattleState;
    friend class FearWormDeadState;

    friend bool RunBeamStateTests();
    friend bool RunFearWormMovementTests();
    friend bool RunFearWormEntryTests();
    friend bool RunFearWormStateTests();

    // 基本設定
    static constexpr float kHeadHp = 40.0f;
    static constexpr float kBaseMoveSpeed = 0.10f;
    static constexpr float kInitialHeadChargeCooldown = 1.80f;
    static constexpr Vector3 kDefaultHeadAimDirection = { 0.0f, 0.0f, -1.0f };

    // ミサイル攻撃と周回速度
    static constexpr float kMissileAimWaitDuration = 2.0f;   // 整列完了（前準備）を待つ時間（秒）
    static constexpr float kMissileAttackInterval = 18.0f;  // ミサイル攻撃の間隔（秒）
    static constexpr float kMissileFireInterval = 0.45f;    // ミサイルの発射間隔（秒）
    static constexpr float kOrbitAngularSpeed = 1.15f;

    // 移動パターン
    static constexpr float kOrbitRadiusX = 16.0f;
    static constexpr float kOrbitRadiusY = 7.0f;
    static constexpr float kOrbitHeight = 7.0f;
    static constexpr float kOrbitLateralSwayFrequency = 2.0f;
    static constexpr float kOrbitLateralSwayAmplitude = 2.0f;
    static constexpr float kOrbitDepthSwayFrequency = 0.80f;
    static constexpr float kOrbitDepthSwayAmplitude = 3.0f;
    static constexpr float kCoilAngleRate = 2.35f;
    static constexpr float kCoilBaseRadius = 8.0f;
    static constexpr float kCoilRadiusFrequency = 3.2f;
    static constexpr float kCoilRadiusAmplitude = 2.0f;
    static constexpr float kCoilHeight = 8.0f;
    static constexpr float kCoilVerticalScale = 0.75f;
    static constexpr float kCoilDepthFrequency = 0.50f;
    static constexpr float kCoilDepthAmplitude = 2.5f;
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

    // 待機中の移動と登場位置
    static constexpr float kFallbackMoveXFrequency = 1.00f;
    static constexpr float kFallbackMoveXAmplitude = 6.0f;
    static constexpr float kFallbackMoveYFrequency = 1.30f;
    static constexpr float kFallbackMoveYAmplitude = 2.5f;
    static constexpr float kFallbackMoveZFrequency = 0.60f;
    static constexpr float kFallbackMoveZAmplitude = 3.0f;
    static constexpr float kEntryOffsetX = -34.0f;
    static constexpr float kEntryOffsetY = 18.0f;
    static constexpr float kEntryExtraForwardOffset = 22.0f;
    static constexpr float kMaximumMovementSpeedAdd = 0.45f;

    // 頭部の溜め攻撃
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

    // 部位のHP・外見・被弾演出
    static constexpr int32_t kWormSegmentCount = 10;
    static constexpr float kBodyHp = 12.0f;
    static constexpr float kBodyScale = 2.45f;
    static constexpr float kHeadScale = 3.45f;
    static constexpr float kTailScale = 2.15f;
    static constexpr float kCollisionRadiusScale = 0.90f;
    static constexpr Vector4 kHeadColor = { 0.30f, 0.88f, 1.0f, 1.0f };
    static constexpr Vector4 kVulnerableHeadColor = { 1.0f, 0.18f, 0.04f, 1.0f };
    static constexpr Vector4 kChargingHeadColor = { 1.0f, 0.48f, 0.08f, 1.0f };
    static constexpr Vector4 kFiringHeadColor = { 1.0f, 0.08f, 0.02f, 1.0f };
    static constexpr Vector4 kBodyColor = { 0.78f, 0.20f, 1.0f, 1.0f };
    static constexpr Vector4 kHitFlashColor = { 1.0f, 0.95f, 0.35f, 1.0f };
    static constexpr float kBodyPulseFrequency = 5.5f;
    static constexpr float kBodyPulseAmplitude = 0.08f;
    static constexpr float kChargePulseAdd = 0.12f;
    static constexpr float kChargePulseFrequency = 24.0f;
    static constexpr float kChargePulseAmplitude = 0.08f;
    static constexpr float kSegmentRotationOffset = 0.35f;
    static constexpr float kDamageHitFlashDuration = 0.10f;
    static constexpr float kGuardHitFlashDuration = 0.08f;
    static constexpr float kMinimumHealth = 0.0001f;

    // 弾幕・ビーム・ミサイル終了時の設定
    static constexpr float kMissilePostFireCooldown = 1.8f; // 撃ち終わった後の隙（秒）
    static constexpr float kPi = 3.1415926535f;
    static constexpr float kBeamUvScrollSpeed = -6.5f; // より高速に流す
    static constexpr float kBeamRotateSpeedRadiansPerSecond = 45.0f; // 高速自転（うねりの表現を強化）
    static constexpr int kBeamDamage = 1;

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
    // 現在の状態を終了し、切替先の開始処理を実行する。
    void ChangeState(BossState state);
    // 撃破後の演出へ切り替え、通常の戦闘処理を終える。
    void OnDeath() override;

    // 激昂中の攻撃と移動を更新し、通常時とは異なる圧力をかける。
    void UpdateMadMode(float movementSpeedRate);
    // 部位からミサイルを撃つ順番と発射タイミングを管理する。
    void UpdateMissileAttack();
    // プレイヤーを基準に移動先を決め、速度倍率を反映して頭を動かす。
    void UpdateBattleMovement(const Vector3& playerPosition, float movementSpeedRate);
    // 戦闘中の攻撃タイマーを進め、選択された攻撃の処理へつなげる。
    void UpdateBattleAttack();

    // 目的地、移動パターン、胴体の追従。
    static Vector3 CalculateOrbitTarget(const FearWormMovementContext& context);
    // 巻き付くように周回する目標位置を求める。
    static Vector3 CalculateCoilTarget(const FearWormMovementContext& context);
    // 左右や上下に揺れながら進む目標位置を求める。
    static Vector3 CalculateWeaveTarget(const FearWormMovementContext& context);
    // ゆっくり漂う移動パターンの目標位置を求める。
    static Vector3 CalculateDriftTarget(const FearWormMovementContext& context);
    // 直線移動パターンの目標位置を求める。
    static Vector3 CalculateLineTarget(const FearWormMovementContext& context);
    // 渦を描く移動パターンの目標位置を求める。
    static Vector3 CalculateSpiralTarget(const FearWormMovementContext& context);
    // 移動の種類に対応する軌道計算を選ぶ。
    static Vector3 CalculateFearWormTarget(FearWormMovementType type, const FearWormMovementContext& context);
    // 次に使用する移動パターンを選ぶ。
    static FearWormMovementType NextFearWormMovement(FearWormMovementType type);
    // 指定軸の周りに回す行列を作り、弾の発射方向の回転などに使う。
    static Matrix4x4 MakeRotateAxisAngleMatrix(const Vector3& axis, float angle);
    // 通常の目標を使えない場合に備え、代わりの移動先を返す。
    Vector3 FallbackTarget();
    // 戦闘用の軌道と経過時間をもとに、現在の追従先を計算する。
    Vector3 BattleTarget(const Vector3& playerPosition, float rate);
    // 登場演出を始める位置をプレイヤー基準で求める。
    Vector3 EntryStartPosition(const Vector3& playerPosition) const;
    // 現在の移動状態に応じた目標位置を取得する。
    Vector3 MovementTargetPosition(const Vector3& playerPosition) const;
    // 移動パターンを変更し、そのパターンで使う経過時間をリセットする。
    void ChangeMovementPattern(MovementPattern pattern);
    // 経過時間を確認し、移動パターンの継続と切替を行う。
    void UpdateMovementPattern(float movementSpeedRate);
    // 頭から後ろへ順に追従位置を計算し、胴体がつながってうねる形を作る。
    void UpdateSegments();
    // 状態や残りHPに応じた移動速度の倍率を返す。
    float MovementSpeedRate() const;

    // 通常弾、チャージ弾、ミサイル、弾幕。
    void Attack() override;
    // 指定した部位の位置から、通常射撃用の弾を生成する。
    void FireBullet(const Vector3& position);
    // 指定した部位から追尾ミサイルを一発生成する。
    void FireSingleMissile(size_t segmentIndex);
    // 頭の溜め攻撃を、準備・発射・終了の順に進める。
    void UpdateHeadChargeAttack(float attackSpeedRate);
    // 頭の溜め攻撃に必要なタイマーと照準状態を準備する。
    void StartHeadChargeAttack();
    // 溜め攻撃の状態を解除し、次の攻撃へ戻れるようにする。
    void FinishHeadChargeAttack();
    // プレイヤーの位置へ頭の狙いを合わせる。
    void UpdateHeadAimDirection();
    // 溜め終わった頭から強化された弾を発射する。
    void FireChargedBullet();
    // 頭の向きと大きさを考慮して、弾を出す位置を求める。
    Vector3 HeadMuzzlePosition() const;
    // 頭が照準方向を向くように描画用の回転角を計算する。
    Vector3 HeadLookRotation() const;
    // 発射角度を回転させながら弾を出し、渦状の弾幕を作る。
    void UpdateSpiralBarrage();
    // 指定された位置と方向を使って弾を生成する。
    void FireDirectionalBullet(const Vector3& position, const Vector3& direction);

    // 部位の生成、描画、当たり判定、HP。
    void InitializeSegments(Model* model);
    // 計算済みの部位位置と向きを、描画用のオブジェクトへ反映する。
    void UpdateSegmentObjects();
    // 胴体が破壊された位置に、破片や爆発の演出を出す。
    void PlayBodyBreakEffect(const Vector3& position);
    // 頭が攻撃を防いだ位置へ、ガードのエフェクトを出す。
    void PlayHeadGuardEffect(const Vector3& position);
    // 頭の弱点に命中した位置へ、ダメージのエフェクトを出す。
    void PlayHeadVulnerableEffect(const Vector3& position);
    // 残りHPの割合を求め、速度や攻撃の変化に使う。
    float HealthRate() const;
    // 生き残っている胴体部位があるかを確認する。
    bool HasAliveBodyParts() const;
    // 部位番号が配列の有効範囲内かを確認し、範囲外アクセスを防ぐ。
    bool IsValidSegmentIndex(int32_t partIndex) const;

    // 待機 → チャージ → 発射 → 終了。
    void InitializeBeam();
    // 頭の位置と狙いにビームを追従させ、ビーム状態を更新する。
    void UpdateBeamAttack();
    // ビーム状態を切り替え、各状態のタイマーや表示を設定する。
    void ChangeBeamState(BeamState state);
    // 待機・溜め・発射・消失のうち、現在のビーム状態を進める。
    void UpdateBeamState(float deltaTime);
    // 次のビームを開始できる条件が整うまで待つ。
    void UpdateBeamWait(float deltaTime);
    // 溜め時間と見た目を更新し、準備が終わったら発射へ移る。
    void UpdateBeamCharge(float deltaTime);
    // 発射時間を進め、時間切れでビームを消す状態へ移る。
    void UpdateBeamFire(float deltaTime);
    // 光線を徐々に消し、次の発射までの待機状態へ戻す。
    void UpdateBeamFadeOut(float deltaTime);
    // 現在のビーム状態に必要な溜め演出や光線を描画する。
    void DrawBeam();
    // 発射中の光線とプレイヤーの距離を調べ、命中時のダメージを処理する。
    void CheckBeamCollision();

    // 各ボスが自分専用の状態オブジェクトを持つ。
    FearWormWaitState waitState_;
    FearWormEntryState entryState_;
    FearWormBattleState battleState_;
    FearWormDeadState deadState_;

    BossState state_ = BossState::Wait;
    FearWormState* currentState_ = nullptr;
    Player* player_ = nullptr;
    Model* bulletModel_ = nullptr;

    std::vector<Segment> segments_;

    Vector3 startPosition_ = { 0.0f, 0.0f, 0.0f };

    float moveTime_ = 0.0f;
    float segmentSpacing_ = 5.0f;
    float bulletSpeed_ = 1.05f;
    float activationLeadDistance_ = 160.0f;
    float parallelForwardOffset_ = 52.0f;
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

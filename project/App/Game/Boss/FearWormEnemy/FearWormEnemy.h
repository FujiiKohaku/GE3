#pragma once

#include "App/Game/Enemy/BaseEnemy.h"

class Model;
class Object3d;
class Player;

class FearWormEnemy : public BaseEnemy {
public:
    enum class BossState {
        Wait,    // プレイヤーを待つ待機状態
        Entry,   // 登場演出状態 (Lerpによるスライドイン)
        Battle,  // 通常戦闘状態 (行動切り替えと攻撃)
        Dead     // 撃破演出状態
    };

    void Initialize(
        Model* model,
        Model* bulletModel,
        Player* player);

    
    void Update() override;

    // 生存中の各部位とボスが発射した弾を描画する。
    void Draw() override;

    // 撃破演出が完了してシーン遷移可能かを返す。
    bool IsDeathSequenceFinished() const;

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
    BossState GetBossState() const { return state_; }
    bool IsMadModeActive() const { return isMadModeActive_; }
    bool IsBeamHittingPlayer() const { return isBeamHittingPlayer_; }

private:
    enum class MovementPattern {
        Orbit, 
        Coil,
        Weave,
        Drift,
        Line,
        Spiral,
    };

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

    // 頭部、胴体、尻尾の各セグメントを生成する。
    void InitializeSegments(Model* model);

    // 各状態の更新関数
    void UpdateWait();
    void UpdateEntry();
    void UpdateBattle();

    // 生存している胴体を1つ前の部位から一定距離に配置する。
    void UpdateSegments();

    // 渦巻き移動と弾幕用
    Vector3 SpiralTargetPosition(const Vector3& playerPosition) const;
    void UpdateSpiralBarrage();
    void FireDirectionalBullet(const Vector3& position, const Vector3& direction);

    // 各セグメントの色、拡縮、回転、座標を描画用オブジェクトへ反映する。
    void UpdateSegmentObjects();

    // ボスが発射した全弾を更新する。
    void UpdateBullets();

    // 寿命切れまたは命中済みの弾をリストから削除する。
    void RemoveDeadBullets();

    // プレイヤーを基準に登場開始位置を計算する。
    Vector3 EntryStartPosition(const Vector3& playerPosition) const;

    // プレイヤー周辺を旋回する目標位置を計算する。
    Vector3 OrbitTargetPosition(const Vector3& playerPosition) const;

    // プレイヤー周辺を巻くように動く目標位置を計算する。
    Vector3 CoilTargetPosition(const Vector3& playerPosition) const;

    // 上下左右へ波打つ目標位置を計算する。
    Vector3 WeaveTargetPosition(const Vector3& playerPosition) const;

    // 緩やかに漂う目標位置を計算する。
    Vector3 DriftTargetPosition(const Vector3& playerPosition) const;

    // 現在選択中の移動パターンに対応する目標位置を返す。
    Vector3 MovementTargetPosition(const Vector3& playerPosition) const;

    // 経過時間に応じて次の移動パターンへ切り替える。
    void UpdateMovementPattern(float movementSpeedRate);

    // ボス戦開始時の登場位置と状態のセットアップを行う。
    void SetupEntryPosition(const Vector3& playerPosition);

    // 通常弾とチャージ攻撃の発射タイミングを更新する。
    void Attack() override;

    // 指定位置からプレイヤー方向へ通常弾を発射する。
    void FireBullet(const Vector3& position);

    // 指定部位から追尾ミサイルを1発撃ち上げる。
    void FireSingleMissile(size_t segmentIndex);

    // チャージ、連射、クールダウンの状態を更新する。
    void UpdateHeadChargeAttack(float attackSpeedRate);

    // 頭部のチャージ攻撃を開始する。
    void StartHeadChargeAttack();

    // チャージ攻撃を終了してクールダウンへ移行する。
    void FinishHeadChargeAttack();

    // 頭部の照準方向をプレイヤー方向へ滑らかに追従させる。
    void UpdateHeadAimDirection();

    // チャージ済みの強化弾を発射する。
    void FireChargedBullet();

    // 頭部位置と照準方向から弾の発射位置を計算する。
    Vector3 HeadMuzzlePosition() const;

    // 照準方向から頭部モデルの回転角を計算する。
    Vector3 HeadLookRotation() const;

    // 胴体が破壊された位置に破壊エフェクトを再生する。
    void PlayBodyBreakEffect(const Vector3& position);

    // 頭部で攻撃を防いだ位置にガードエフェクトを再生する。
    void PlayHeadGuardEffect(const Vector3& position);

    // 全胴体破壊後に頭部が弱点化したことを示すエフェクトを再生する。
    void PlayHeadVulnerableEffect(const Vector3& position);

    // 尻尾側から順に部位を爆発させる撃破演出を更新する。
    void UpdateDeathSequence();

    // 全部位の現在HPからボス全体の残りHP割合を計算する。
    float HealthRate() const;

    // 残りHPに応じた移動・攻撃速度の倍率を計算する。
    float MovementSpeedRate() const;

    // 生存している胴体部位が1つでもあるかを返す。
    bool HasAliveBodyParts() const;

    // 部位番号がセグメント配列の有効範囲内かを返す。
    bool IsValidSegmentIndex(int32_t partIndex) const;

    // 撃破直後に弾と攻撃状態を停止し、撃破演出を開始する。
    void OnDeath() override;

    // 各状態（フェーズ）の目的地を計算するヘルパー関数
    Vector3 FallbackTarget();                                      // プレイヤー不在時の目的地
    Vector3 EntryTarget(const Vector3& playerPosition);             // 登場演出中の目的地
    Vector3 BattleTarget(const Vector3& playerPosition, float rate);// 戦闘中の目的地

    // ビーム攻撃用メンバー関数
    enum class BeamState {
        Wait,    // 次のビームまでの待機（クールダウン）
        Charge,  // 発射前の予兆（マズルフラッシュと強めの追従）
        Fire,    // ビーム発射（3秒照射、弱めの追従、当たり判定）
        FadeOut  // 照射終了（ビーム細小化フェード）
    };
    void InitializeBeam();
    void UpdateBeamAttack();
    void DrawBeam();
    void CheckBeamCollision();


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
    BossState state_ = BossState::Wait;

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
};


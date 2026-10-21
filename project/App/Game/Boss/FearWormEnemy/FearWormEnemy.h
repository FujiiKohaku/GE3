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
    bool IsMadModeActive() const override { return battleState_.IsMadModeActive(); }
    bool IsBeamHittingPlayer() const override { return battleState_.IsBeamHittingPlayer(); }

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
    friend class FearWormOrbitShotState;
    friend class FearWormWeaveSpreadState;
    friend class FearWormDriftBurstState;
    friend class FearWormCoilBarrageState;
    friend class FearWormHeadChargeState;
    friend class FearWormLineMissileState;
    friend class FearWormSpiralBarrageState;
    friend class FearWormBeamState;

    friend class FearWormDeadState;

    friend bool RunBeamStateTests();
    friend bool RunFearWormMovementTests();
    friend bool RunFearWormEntryTests();
    friend bool RunFearWormStateTests();
    friend bool RunFearWormBattleActionTests();

    // 基本設定
    static constexpr float kHeadHp = 40.0f;
    static constexpr float kBaseMoveSpeed = 0.10f;

    static constexpr float kMaximumMovementSpeedAdd = 0.45f;

    // 部位のHP・外見・被弾演出
    static constexpr int32_t kWormSegmentCount = 10;
    static constexpr float kBodyHp = 12.0f;
    static constexpr float kBodyScale = 2.45f;
    static constexpr float kHeadScale = 3.45f;
    static constexpr float kTailScale = 2.15f;
    static constexpr float kCollisionRadiusScale = 0.90f;
    static constexpr Vector4 kHeadColor = { 0.30f, 0.88f, 1.0f, 1.0f };
    static constexpr Vector4 kVulnerableHeadColor = { 1.0f, 0.18f, 0.04f, 1.0f };
    static constexpr Vector4 kBodyColor = { 0.78f, 0.20f, 1.0f, 1.0f };
    static constexpr Vector4 kHitFlashColor = { 1.0f, 0.95f, 0.35f, 1.0f };
    static constexpr float kBodyPulseFrequency = 5.5f;
    static constexpr float kBodyPulseAmplitude = 0.08f;
    static constexpr float kSegmentRotationOffset = 0.35f;
    static constexpr float kDamageHitFlashDuration = 0.10f;
    static constexpr float kGuardHitFlashDuration = 0.08f;
    static constexpr float kMinimumHealth = 0.0001f;

    using FearWormMovementType = FearWormBattleState::FearWormMovementType;
    using FearWormMovementContext = FearWormBattleState::FearWormMovementContext;

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

    // 移動の種類に対応する軌道計算を選ぶ。
    static Vector3 CalculateFearWormTarget(FearWormMovementType type, const FearWormMovementContext& context);
    // 現在の移動状態に応じた目標位置を取得する。
    Vector3 MovementTargetPosition(const Vector3& playerPosition) const;
    // 頭から後ろへ順に追従位置を計算し、胴体がつながってうねる形を作る。
    void UpdateSegments();
    // 状態や残りHPに応じた移動速度の倍率を返す。
    float MovementSpeedRate() const;

    // 頭が照準方向を向くように描画用の回転角を計算する。
    Vector3 HeadLookRotation() const;

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
    // 現在のビーム状態に必要な溜め演出や光線を描画する。
    void DrawBeam();

    // 各ボスが自分専用の状態オブジェクトを持つ。
    FearWormWaitState waitState_;
    FearWormEntryState entryState_;
    FearWormBattleState battleState_;
    FearWormDeadState deadState_;

    BossState state_ = BossState::Wait;
    FearWormState* currentState_ = nullptr;
    Player* player_ = nullptr;

    std::vector<Segment> segments_;

    float moveTime_ = 0.0f;
    float segmentSpacing_ = 5.0f;
    float parallelForwardOffset_ = 92.0f;

    bool vulnerableEffectPlayed_ = false;

};

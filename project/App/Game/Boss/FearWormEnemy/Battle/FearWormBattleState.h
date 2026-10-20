#pragma once

#include "../State/FearWormState.h"
#include "Engine/Math/MathStruct.h"
#include <cstddef>

// 戦闘中の移動・攻撃・ビームの実処理を担当する。
class FearWormBattleState final : public FearWormState {
public:
    void Update(FearWormEnemy& boss) override;

private:
    friend class FearWormEnemy;

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

    // 激昂中の攻撃と移動を更新し、通常時とは異なる圧力をかける。
    static void UpdateMadMode(FearWormEnemy& boss, float movementSpeedRate);
    // 部位からミサイルを撃つ順番と発射タイミングを管理する。
    static void UpdateMissileAttack(FearWormEnemy& boss);
    // プレイヤーを基準に移動先を決め、速度倍率を反映して頭を動かす。
    static void UpdateBattleMovement(FearWormEnemy& boss, const Vector3& playerPosition, float movementSpeedRate);
    // 戦闘中の攻撃タイマーを進め、選択された攻撃の処理へつなげる。
    static void UpdateBattleAttack(FearWormEnemy& boss);
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
    static Vector3 BattleTarget(FearWormEnemy& boss, const Vector3& playerPosition, float rate);
    // 現在の移動状態に応じた目標位置を取得する。
    static Vector3 MovementTargetPosition(const FearWormEnemy& boss, const Vector3& playerPosition);
    // 経過時間を確認し、移動パターンの継続と切替を行う。
    static void UpdateMovementPattern(FearWormEnemy& boss, float movementSpeedRate);
    // 移動パターンを変更し、そのパターンで使う経過時間をリセットする。
    static void ChangeMovementPattern(FearWormEnemy& boss, MovementPattern pattern);
    // 射撃タイミングを管理し、通常弾や特殊攻撃を開始する。
    static void Attack(FearWormEnemy& boss);
    // 頭の溜め攻撃を、準備・発射・終了の順に進める。
    static void UpdateHeadChargeAttack(FearWormEnemy& boss, float attackSpeedRate);
    // 頭の溜め攻撃に必要なタイマーと照準状態を準備する。
    static void StartHeadChargeAttack(FearWormEnemy& boss);
    // 溜め攻撃の状態を解除し、次の攻撃へ戻れるようにする。
    static void FinishHeadChargeAttack(FearWormEnemy& boss);
    // プレイヤーの位置へ頭の狙いを合わせる。
    static void UpdateHeadAimDirection(FearWormEnemy& boss);
    // 指定した部位の位置から、通常射撃用の弾を生成する。
    static void FireBullet(FearWormEnemy& boss, const Vector3& position);
    // 溜め終わった頭から強化された弾を発射する。
    static void FireChargedBullet(FearWormEnemy& boss);
    // 頭の向きと大きさを考慮して、弾を出す位置を求める。
    static Vector3 HeadMuzzlePosition(const FearWormEnemy& boss);
    // 頭が照準方向を向くように描画用の回転角を計算する。
    static Vector3 HeadLookRotation(const FearWormEnemy& boss);
    // 指定した部位から追尾ミサイルを一発生成する。
    static void FireSingleMissile(FearWormEnemy& boss, size_t segmentIndex);
    // 任意軸まわりの回転行列を生成する関数 (ロドリゲスの回転公式)
    static Matrix4x4 MakeRotateAxisAngleMatrix(const Vector3& axis, float angle);
    // 発射角度を回転させながら弾を出し、渦状の弾幕を作る。
    static void UpdateSpiralBarrage(FearWormEnemy& boss);
    // 指定された位置と方向を使って弾を生成する。
    static void FireDirectionalBullet(FearWormEnemy& boss, const Vector3& position, const Vector3& direction);
    // ビーム本体と溜め演出を描画するリソースを準備する。
    static void InitializeBeam(FearWormEnemy& boss);
    // 頭の位置と狙いにビームを追従させ、ビーム状態を更新する。
    static void UpdateBeamAttack(FearWormEnemy& boss);
    // 現在のビーム状態に必要な溜め演出や光線を描画する。
    static void DrawBeam(FearWormEnemy& boss);
    // 発射中の光線とプレイヤーの距離を調べ、命中時のダメージを処理する。
    static void CheckBeamCollision(FearWormEnemy& boss);
    // 待機・溜め・発射・消失のうち、現在のビーム状態を進める。
    static void UpdateBeamState(FearWormEnemy& boss, float deltaTime);
    // 次のビームを開始できる条件が整うまで待つ。
    static void UpdateBeamWait(FearWormEnemy& boss, float);
    // 溜め時間と見た目を更新し、準備が終わったら発射へ移る。
    static void UpdateBeamCharge(FearWormEnemy& boss, float dt);
    // 発射時間を進め、時間切れでビームを消す状態へ移る。
    static void UpdateBeamFire(FearWormEnemy& boss, float dt);
    // 光線を徐々に消し、次の発射までの待機状態へ戻す。
    static void UpdateBeamFadeOut(FearWormEnemy& boss, float dt);
    // ビーム状態を切り替え、各状態のタイマーや表示を設定する。
    static void ChangeBeamState(FearWormEnemy& boss, BeamState state);
};

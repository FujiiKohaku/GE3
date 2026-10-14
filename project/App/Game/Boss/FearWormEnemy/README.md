# FearWormの読み方

FearWorm専用の状態・移動・ビームに継承は使わない。
`FearWormEnemy`がデータを持ち、enumと普通の関数で処理を選ぶ。
`BaseBoss`との継承は、ゲーム側が各ボスを共通に扱うために残している。

## ファイル

| ファイル | 担当 |
| --- | --- |
| `FearWormEnemy.h` | 関数の宣言とデータ。privateの関数は実装ファイルごとに並んでいる |
| `FearWormEnemy.cpp` | 初期化、毎フレームの入口、状態の選択と切り替え |
| `FearWormEnemyStates.cpp` | 待機・登場・戦闘・死亡の進行 |
| `FearWormEnemyMovement.cpp` | 目的地の補正、移動パターンの切り替え、胴体の追従 |
| `FearWormMovement.h/.cpp` | 6種類の軌道の計算式。状態を持たない普通の関数 |
| `FearWormEnemyAttack.cpp` | 通常弾・チャージ弾・ミサイル・弾幕の発射 |
| `FearWormEnemyBeam.cpp` | ビームの進行、形状、描画、当たり判定 |
| `FearWormEnemyParts.cpp` | 部位の生成、見た目、描画、ダメージ、HP |

## 生成時

```text
BossEncounterController::Update()
  出現距離に達したらSpawnBoss()
    FearWormBossFactory::Create()
      FearWormEnemy()             初期状態はWait
      Initialize()
        InitializeSegments()     頭・胴体・尾を生成
        InitializeBeam()         ビームを準備
        SetPosition()            位置と行動を初期化
    SetPosition(spawnPosition_)  出現位置へ配置
```

## 毎フレーム（ここから読む）

```text
BossEncounterController::Update()
  FearWormEnemy::Update()                         [FearWormEnemy.cpp]
    死亡していたらChangeState(Dead)
    moveTime_を更新
    UpdateState()                                switchで1つ選ぶ
      Wait   → UpdateWait()                      [FearWormEnemyStates.cpp]
      Entry  → UpdateEntry()
      Battle → UpdateBattle()
      Dead   → UpdateDead()
    UpdateSegments()                             [FearWormEnemyMovement.cpp]
    UpdateSegmentObjects()                       [FearWormEnemyParts.cpp]
    全胴体破壊時の弱点化エフェクト
```

描画は更新とは別の呼び出し。`Draw()`から部位とビームを描く。
影・レイトレーシング用には別の描画入口がある。

## 戦闘中

```text
UpdateBattle()
  UpdateMadMode()          発狂の開始・終了、回転角度の更新
  UpdateMissileAttack()    整列準備 → 順番に発射 → 硬直
  UpdateBattleMovement()
    BattleTarget()
      UpdateMovementPattern()
      カメラに合わせて高度を補正
      MovementTargetPosition()
        CalculateFearWormTarget()  switchで軌道の計算式を選ぶ
    頭を目的地へ近づける
  UpdateBattleAttack()
    頭だけならビームを更新
    それ以外で発狂中なら弾幕
    それ以外でミサイル動作中でなければ通常攻撃
```

通常の移動は `Orbit → Coil → Weave → Drift → Orbit`。
ミサイル動作中は`Line`、発狂中は`Spiral`を選び、これらは時間で自動切り替えしない。
移動先の計算と、実際に頭を動かす処理は別。

## 状態を切り替える瞬間

`ChangeState()`はenumを書き換える。同じ状態なら何もしない。
Entryへ切り替えたときは`EnterEntry()`、Deadへ切り替えたときは`EnterDead()`をその場で呼ぶ。
新しい状態の`Update`は通常、次のフレームに実行される。

ビームも`UpdateBeamState()`のswitchで
`Wait → Charge → Fire → FadeOut → Wait`を選ぶ。
`ChangeBeamState()`で段階を変更するとタイマーを0に戻し、必要な開始処理を行う。
Wait中は通常攻撃も行う。

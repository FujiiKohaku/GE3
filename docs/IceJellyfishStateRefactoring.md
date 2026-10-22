# IceJellyfish の大きな状態の分離

2026-10-21。フィアーボスと同じくWait・Entry・Battle・Deadの4状態へ分離した。登場演出は空にする、というユーザー指定に従う。

| 所有者 | 責務 |
| --- | --- |
| IceJellyfish本体 | 状態の所有・遷移、共通アニメーション時計、部位姿勢の更新、描画・HP・衝突の窓口 |
| WaitState | 出現後の入口。最初の更新でEntryへ進む |
| EntryState | 演出なしでBattleへ進む |
| BattleState | 浮遊移動、3攻撃の開始順序・開始判定、3攻撃の並行更新 |
| DeadState | 死亡経過秒、毎秒Y方向-9・Z方向+5の移動、2秒での演出完了判定 |

出現距離は従来どおりBossEncounterControllerが判定する。Waitと空のEntryは初回更新内で通過させ、従来と同じフレームから戦闘を1回更新する。状態遷移時のEnter・Exitを経由し、同じ状態の再指定では初期化しない。

BattleStateは開始する攻撃の番号と、浮遊周期の開始判定フラグを所有する。死亡タイマーはDeadStateへ移し、本体の描画終了判定も同じ問い合わせ関数を使用する。死亡通知と通常更新からの死亡確認が重なっても、タイマーはリセットしない。

各Stateは各ボスがメンバーとして所有し、切り替え時の動的確保は行わない。通常更新はStateへの仮想関数呼び出しが加わる。部位更新・座標計算・弾生成の回数は増やしていない。性能の実測は未実施。

## 今回の範囲

3攻撃の具体的な発射・予告・命中処理と、それぞれのタイマーは本体に残す。旧6パターンも既存の初期化・描画・判定・テストの関係を維持する。まず大きな状態を整理し、攻撃ごとの分離は後続の変更として扱う。

## 検証

- App全68 CPPをDevelopment相当設定でコンパイル：成功。
- 新規IceJellyfishStateTestsのビルド・実行：成功。4状態の接続、初回更新での戦闘開始、全体時計・3攻撃の時計の単一更新、浮遊座標、30・60・120FPSの死亡移動、停止、死亡通知の繰り返し、2秒ちょうどの完了判定、待機・登場からの死亡、初期化と別個体への非干渉を確認。
- 既存JellyfishAttackTests・JellyfishAttackPatternTestsを同じ実行ファイルで実行：成功。現行3攻撃の開始順序・並行更新と、旧パターンの切り替えを確認。
- 既存IceJellyfishCollisionTestsをDebug設定でビルド・実行：18件成功。
- FearWorm回帰テストを現在のApp成果物で再ビルド・実行：成功。
- プロジェクト・filters・テストプロジェクトのXMLと、差分の空白エラーを確認。

テストは描画資源を生成せずに状態・時間・座標・判定を検証する。ゲーム全体のビルドと実画面での確認は今回実施していない。

ログ：`generated/IceJellyfishStatesCompile.log`、`generated/IceJellyfishStatesTestsBuild.log`、`generated/IceJellyfishStatesCollisionBuild.log`、`generated/IceJellyfishStatesFearWormTestsBuild.log`。

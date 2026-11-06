# FPS低下監視

`FpsMonitor` はDebug／Releaseの両方で動く専用クラスです。
`Game::Update()`から`Game::Draw()`までとフレーム間の空白を実時間で計測します。
`TimeManager`の時間倍率・最大デルタ制限には影響されません。
現在の集計FPSは`FpsMonitor::GetInstance().GetFps()`、低下中かどうかは
`IsLowFps()`で取得できます。

## 設定

`project/resources/Config/FpsMonitor.json`を編集してゲームを再起動します。

| キー | 初期値 | 意味 |
| --- | ---: | --- |
| enabled | true | 監視を有効化 |
| thresholdFps | 50 | このFPS未満を低下として判定 |
| sampleSeconds | 0.5 | FPSと処理時間を集計する区間 |
| lowFpsSeconds | 1.0 | 低い区間がこの時間続いたら通知 |
| cooldownSeconds | 5.0 | 診断ログの最小間隔 |
| warmupSeconds | 3.0 | 最初のフレームからの除外時間 |
| spikeMilliseconds | 100 | 単発の長いフレーム。0で無効 |
| recoveryMarginFps | 5 | しきい値＋この値以上で回復通知 |
| topSections | 6 | 時間の大きいCPU区間の表示数 |

例えば45 FPSを下限にしたい場合は`thresholdFps`を45にします。
FPS上限に近い値をしきい値にする場合、回復条件も上限内になるよう
`recoveryMarginFps`を調整してください。

## ログ

既存の`project/generated/logs/EngineLog_日時.txt`へ出力します。
`[FpsMonitor][LowFPS]`、`[FpsMonitor][FrameSpike]`、`[FpsMonitor][Recovered]`
で検索できます。通常は低下／回復時だけ記録します。
Debugの旧エフェクト用FrameSpike自動出力はこの監視に集約し、毎フレームの
エフェクト計測カウンターのリセットと既存の毎秒Performance集計は維持しています。

診断には次を含みます。

- シーン名・ステージID、FPS、フレーム時間、集計フレーム数。
- CPU側の区間別平均・最大時間と、区間内で最も遅かったフレームの内訳。
- 更新、敵更新、当たり判定、エフェクト、モデル／テクスチャ／シェーダー読み込み。
- 3D／影／パーティクル／UIの描画コマンド作成、キャプチャの読み戻し・画像化。
- Presentの待ち時間、GPUフェンスの待ち時間、フレーム間の未分類時間。
- GPUフレーム時間、利用できる場合のDLAA時間。
- 敵・敵弾・ステージオブジェクト・アクティブエフェクト数の区間内最大値。

`exclusive`は子区間を除いた時間、`inclusive`は子区間を含む時間です。
ランキングはexclusiveで作り、入れ子の時間を二重加算しません。
GPUのフレーム時間とDLAA時間も重なるため、別々の参考値として出します。

## 原因の読み取り方

`candidate`は計測結果に基づく候補です。呼び出し履歴やハードウェアの
因果関係まで自動で断定する機能ではありません。

- `CPU-side Collision.PlayerBullets`：プレイヤー弾の衝突処理に長い経過時間。
- `GPU workload`：実測のGPUフレーム時間が設定した時間予算を超えている。
- `Present/VSync or GPU fence wait`：待ちが大きい。GPU負荷とは断定しない。
- `frame gap/... (unattributed)`：メッセージ処理、ログ出力など未分類の時間。
- `GPU frame=unavailable`：有効なGPU計測値がない。0 msとは扱わない。

CPU計測はメインスレッド上の経過時間です。OSによるスケジュール待ちや
ドライバー内部の待ちも含み、CPU使用率とは異なります。
CPUとGPUは並行するため、両方の時間を足してフレーム時間にはしません。
GPU計測には既存のフェンス完了後のタイムスタンプ読み戻しを使い、監視のための
追加GPU待ちは行いません。Releaseではフレーム全体とDLAAのタイマーだけを追加で有効にします。

## 計測区間を追加する

```cpp
#include "Engine/Debug/Profiler/FpsMonitor.h"

{
    FpsScope scope("Gameplay.MyProcessing");
    // 調べたい処理
}
```

固定の区間名を使ってください。登録上限は64区間、入れ子は32段、GPU詳細と
個数情報は各12項目です。通常フレームでは文字列の整形やログ出力をしません。
ゲームのメインスレッド以外のScopeは記録せず、共有計測スタックを壊さないようにしています。
起動直後、最小化中、終了確認ダイアログを表示したフレームは判定から除外します。
シーンが変わった場合は、前のシーンの低下継続時間を引き継ぎません。

## テスト

`Tests/Projects/FpsMonitorTests.vcxproj`をDebug x64でビルドし、
`generated/outputs/Debug/FpsMonitorTests.exe`を実行します。
偽の時計を使い、実際に処理を遅くせずに継続判定、ログ間隔、回復、
入れ子、フレーム間の空白、GPU欠測、起動時除外、設定を検証します。

# HDR・自動露出・トーンマッピング

## 機能とクラス

- `AutoExposureRenderer`: GPUで256ビンの対数輝度ヒストグラムを作成し、外れ値を除いた平均から露出を決める。最終フレームのHDR画像をBloom合成前に計測する。手動露出とEV補正は自動露出の倍率に掛ける。
- `PostEffectManager`: 自動露出を最終描画に接続する。カメラ交換、`Camera::ResetMotionHistory`、シーン露出revision、ON/OFFや計測設定変更で適応履歴をリセットする。
- `CopyImageRenderer`: 最終ToneMap/FXAAの両方で露出を適用する。線形HDRから最終sRGB RTVへの変換は1回。中間色とBloomはRGBA16_FLOATを維持する。フレア・光芒・粒子感・レンズ汚れなど12種類の中間エフェクトの出力も1で切り捨てず、半精度の有限範囲を保持する。
- `BloomRenderer`: soft kneeと抽出放射輝度の上限を公開する。輝度上限はRGB共通倍率で適用する。合成結果を半精度の有限範囲に収め、NaN/Infの色を無効化する。

既定は自動露出OFF、従来トーン方式。開発パネルのHDRグループで「自動露出」、トーン方式1を選ぶ。DebugのImGuiにも操作を公開している。

```cpp
AutoExposureSettings settings;
settings.isEnabled = true;
postEffects.GetAutoExposureRenderer()->SetSettings(settings);
postEffects.SetToneMapping(1, 0.0f);
```

方式0は既存のRGB各成分のフィルミック近似。方式1は明るさを共通倍率で圧縮する色保持方式で、最大成分が1に漸近する肩を持つ。AgX/ACESの色管理一式ではない。`toneContrast=1`、`toneSaturation=1` なら線形RGB比率を保つ。個別のコントラスト・彩度設定はその後に適用する。

## 自動露出

輝度はRec.709重み。log2輝度範囲は-12〜16、黒・非有限値をビン0に分け、平均から除く。既定で下位5%と上位5%を除いて対数平均を求め、middle gray 0.18に対応する露出を算出する。真っ黒な画像は露出1を範囲内に収める。

既定の露出範囲は1/64〜64。暗所への適応速度1/秒、明所への適応速度3/秒。`1-exp(-speed*deltaSeconds)` の重みを露出のlog2空間に適用し、フレーム数による差を抑える。初回・履歴リセット時は目標値に直接設定する。非有限・負のdeltaは0、1秒を超える値は1秒に制限する。

公開`Generate`は入力テクスチャがPIXEL_SHADER_RESOURCEの状態で呼ぶ。完了時は同じ状態に戻し、結果もPIXEL_SHADER_RESOURCEで公開する。同じインスタンスを1フレームに複数回実行する使い方は対象外。既存エンジンのフレーム完了待ちを前提とする。CPUへの輝度読戻しや追加の同期待ちは行わない。

## コスト

ON時はクリア、画面ヒストグラム、露出集計のCompute dispatch3回。1280×720では921,600画素を1回読む。ヒストグラムは各16×16グループ内で集計し、非ゼロビンだけを全体に加算する。露出画像はR32_FLOATの1×1を2枚、ヒストグラムは1 KiB。割当単位を含めた既定ヒープ192 KiB、アップロード64 KiB、計256 KiB。デバッグ用タイムスタンプ資源・共通ディスクリプタヒープ・ドライバー内部の容量は含まない。

OFFではdispatchしない。初回ONまで専用GPU資源は作らず、作成後のOFFは再利用用に保持する。露出・色保持・Bloom改善でレイや追加のフルスクリーン描画パスは増えない。色保持方式の計算と、最終描画/FXAA内の露出スカラー参照が増える。

2026-10-10、RTX 4060 Laptop GPU、Debug、1280×720の単独検証で計測処理0.128 ms。実ゲームの性能保証ではない。

## 検証

`DxrTests.exe --hdr` は逆輝度露出、時間による適応、ゼロdelta、分割時間の一致、明るい外れ値の除外、黒・露出範囲・不正設定・OFFを検証する。実際の最終描画から画素を読み、自動露出・色圧縮・sRGB変換と手動への復帰、FXAAとの併用、EV補正も検証する。12種類の中間エフェクトがHDR値を保持すること、BloomのHDRエネルギー、抽出上限、半精度出力の有限性を確認する。D3D12のERROR/CORRUPTIONは失敗扱い。

結果は `runtime/captures/DxrTests/hdr-result.txt` とPNG。露出約0.045の自動描画では最終値0.533333（期待sRGB 0.535052）、手動描画では0.972549。Bloom合成はHDR値7を保持し、非常に強い入力でも最大65504で有限だった。

検証結果：HDR重点検証、DXR全件検証、`ShadowMapTests --volumetric` による既存の影・大気・霧・ポスト処理の重点検証、Developmentビルドは成功。最後に追加した12種類の中間HDR修正はHDR重点検証とShadowMap重点検証で確認した。既存の `ShadowMapTests` 全件にある半球光の画像比較失敗は別件として未解決。

# 汎用TAA・時間方向アップスケーラー

`TemporalSuperResolution` を追加した。NVIDIA NGXを使わず、D3D12 ComputeでHDRカラーを再構成する。通常描画にネイティブTAAと低解像度入力の復元を接続し、借用したカラー・深度・モーションから表示解像度の画像を作るAPIを提供する。

## 操作と描画順

Debugの「Anti-aliasing comparison」またはDevelopmentの「アンチエイリアス比較」で「TAA」「TAA + FXAA」を選ぶ。既存のOFF/FXAA/DLAA/DLAA + FXAAも選べる。DLAAとTAAは同時に評価しない。

```cpp
renderer.SetTemporalAntiAliasing(true, false);
```

TAAはHDRのシーン・反射・霧合成後、粒子・HUD・画面歪み・Bloom・自動露出・最終トーンマッピングの前に実行する。既存のモーションベクトルと物体の前深度/IDを利用する。粒子とHUDは時間蓄積に入れない。

## 再構成

- 32段階のHaltonジッター。呼び出し側が`GetProjectionJitterNdc`をカメラに適用する。
- 入力座標をジッター分ずらして、出力はジッターを除いた座標に再構成する。色は双線形、または16タップCatmull-Rom。入力近傍の色範囲に制限し、負のローブによるリンギングを抑える。
- 近傍で最も手前の深度のモーションを用い、前フレームへ投影する。履歴の各4タップを、前深度・物体ID・画面範囲・有限性で判定してから補間する。
- HDRを圧縮したYCoCg空間で3×3近傍の平均・分散を計算し、履歴の色を制限する。色の急変時は履歴の比率を下げる。
- 空はカメラの方向を前フレームの射影へ投影し、平行移動を除く。空と物体が切り替わった画素の履歴は捨てる。
- カメラ交換/履歴ID、シーン、照明revision、設定・入力サイズ変更、欠落入力で履歴をリセットする。Rendererでは太陽・天空・大気の急変と既存照明モードの変更も通知する。緩やかな照明変化は履歴を維持する。判定と検証は [最終画質検証](../Renderer/FinalQualityValidation.md)。

既定historyWeightは0.9、varianceGammaは1.25。`HasUsedHistory()`はCPU側で履歴の再利用を試みたことを示し、各画素が履歴を採用したことまでは示さない。

## 低解像度入力

`TemporalResolutionSettings` の inputWidth/inputHeight/outputWidth/outputHeight に入力と出力のサイズを指定する。同じ縦横比で出力を入力以上のサイズにする。出力は最大3840×2160。カラー・深度・モーション・任意の前深度/IDは入力解像度にそろえる。出力はRGBA16_FLOAT、履歴ガイドはRG32_FLOAT。

全入力をPIXEL_SHADER_RESOURCEとして渡し、同じ状態で返す。`BeginFrame`→ジッター適用→入力描画→`Evaluate`の順。サイズ変更/資源作成・解放・アップロード再使用は既存エンジンのフレーム完了待ちを前提とする。

通常ゲームにも低解像度入力を接続した。`Renderer::SetLowResolutionRendering` とTAAの設定は独立し、低解像度OFFでもネイティブTAA、TAA OFFでも空間拡大を使える。表示は1280×720のまま。詳細は [RenderResolution.md](../Renderer/RenderResolution.md)。DLSS/FSRの品質や性能と同等であるとは評価していない。

## コスト

1280×720の履歴画像はRGBA16_FLOAT×2とRG32_FLOAT×2で実割当30 MiB。定数アップロード・デバッグ計測・共通ディスクリプタ・ドライバー内部の容量は別。初回有効な入力まで履歴画像を作らない。OFFは評価せず、ジッターを0にし、作成済み資源を再利用用に保持する。

評価はCompute dispatch1回。カラー再構成の最大16サンプル、近傍色/深度各9サンプル、履歴ガイド/色の各4タップなどを出力画素ごとに処理する。通常描画では既存の粒子合成先へのコピー1回も必要。追加のレイはない。

2026-10-10、RTX 4060 Laptop GPU、Debug、1280×720の単独検証では評価約1.068 ms、画像31,457,280 bytes。コピー、モーション描画、実シーン全体の時間は含まない。少数フレームの検証値であり性能保証ではない。

## 検証

`DxrTests.exe --taa` はHDR保持、安定した履歴、縞のちらつき、深度/物体ID/画面外投影の拒否、色の急変、カメラ/シーン/照明リセット、空、欠落入力、OFFを検証する。32×18のHDR入力を64×36に再構成する実GPU検証も行う。

縞のMSEは現在フレーム0.0835037→時間再構成0.00652883（約92%低減）。2倍拡大したHDRランプは2.06445、期待値2.06452。これは人工的な縞/ランプのテストで、全場面の画質改善率を示すものではない。

`ShadowMapTests.exe --taa` は実際のタイトル・ステージ03、ネイティブTAA、カメラカット、FXAA併用、DLAAとの排他的選択、OFF、D3D12検証を確認する。結果は `runtime/captures/DxrTests/taa-result.txt` と `runtime/captures/ShadowMapTests/taa-integration-result.txt`。

TAA基盤追加時の検証結果：TAA重点検証、タイトル/ステージ03の統合検証、既存NGX DLAAの回帰検証、DXR全件検証、Developmentビルドは成功。実ステージのTAA評価は約1.084 msだった。DXR全件検証の最後に同じネイティブ評価を行うと7.228 msを記録したため、単独値を固定の性能保証には使わない。当時残っていた `ShadowMapTests` 全件の半球光比較失敗は、最終画質検証で環境マップとの置き換えを分離した検証条件に修正して解消した。

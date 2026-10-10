# DLAA

GPUメーカーに依存しない追加のTAAと、低解像度入力の再構成APIは [汎用時間再構成](TemporalResolution.md) を参照。通常ゲームはネイティブTAAと、低解像度の3D入力から表示解像度への復元を選択できる。

NVIDIA公式NGX SDKをD3D12へ直接接続しています。固定コミットは `externals/DLSS/SDK-COMMIT.txt`、利用条件は同ディレクトリの `LICENSE.txt` にあります。MSBuildが `nvngx_dlss.dll` を実行ファイルと同じディレクトリへコピーします。

初期状態はONです。Debugの「Super resolution」で「DLAA (Native resolution)」を切り替えられます。コードからは `Renderer::GetDlssSuperResolution()->SetEnabled(true)` を使います。利用可否は `GetStatus()` で確認できます。非対応環境では通常描画を維持し、評価失敗時はポスト合成済みのカラーをそのままコピーして表示します。

## 描画順

1. 履歴更新と投影ジッター設定
2. シャドウ・3Dカラー・深度・法線・モーションベクトル描画
3. SSAO・大気遠近・立体霧の合成
4. BeforeParticle指定の輪郭線・距離霧等の空間的な処理
5. DLAAのEvaluate
6. 出力を粒子用ターゲットへコピーし、投影ジッター解除
7. 粒子描画
8. 画面揺れ・歪み・ぼかし等の演出、AfterParticle指定の処理
9. Bloom、トーンマッピング、有効ならFXAA
10. HUD・UI・画面表示

`IsTemporalResolveInputEffect()` が、BeforeParticle指定の効果をDLAA前へ入れられるかを判定します。Copy、ScreenLighting、DepthOutline、LuminanceBasedOutline、Outline、Fogが対象です。それ以外のBeforeParticle指定の効果は粒子描画後へ送ります。明示的なAfterParticle指定は維持します。新しい効果は判定に追加するまでDLAA後になります。DLAAの前の画像を動かす処理は、深度やモーションと対応しなくなるため後へ送ります。

DLAAは表示と同じ1280×720で3Dを描画し、合成済みHDRカラー、元の深度、モーション、ジッターをNGXへ渡します。モーションはジッターを除いた現在−過去のUV値を、過去−現在のピクセル値へ変換します。シーン切り替え、有効化、カメラ履歴リセットでDLAAの履歴もリセットします。カメラカットには `Camera::ResetMotionHistory()` を使ってください。カメラ入力はジッター設定後に更新するため、スクリーンライティングの深度復元も同じ射影で行います。

## 汎用入力

DLAAはMotionVectorRenderer、OffscreenRenderer、PostEffectManager、Camera、TimeManagerへ直接依存しません。生成側と実行側は独立しており、Rendererが入力と実行順を管理します。SuperResolutionFrameInputsにカラー・深度・モーションのテクスチャ、カラーSRV、フレーム時間を渡します。法線はDLAAの入力から外し、ポスト処理で使用します。

BeginFrameにSuperResolutionHistoryInputsを渡し、GetProjectionJitterNdcを呼び出し側のカメラへ反映します。Evaluateは借用リソースを受け取り、処理結果のカラーSRVを返します。入力はすべてPIXEL_SHADER_RESOURCEで渡し、同じ状態へ戻します。深度とモーションの内容は変更しません。出力もPIXEL_SHADER_RESOURCEです。

サイズは固定1280×720、カラーはRGBA16_FLOAT、深度はR24G8_TYPELESS、モーションはRG16_FLOATです。入力の所有権は受け取りません。出力SRVは所有するDLAAオブジェクトの生存中に限り使用できます。EvaluateはNGXによりコマンドリストのバインド状態を変更します。後続の描画はルートシグネチャ・PSO・ビュー等を設定してください。

## コストと検証

追加の出力テクスチャは1280×720で約7.0 MiBです。NGX内部リソースとモーション用リソースは別途必要です。旧実装の深度・法線コピーと補助拡大パスを廃止し、DLAAの後に粒子用ターゲットへHDRカラーをコピーする1パスを追加しています。OFFまたは非対応時は同じ入力SRVを返し、このコピーも省きます。GPU時間は未測定です。

透明物・空・アルファカットアウトの正確なモーション、任意ウィンドウサイズは未対応です。霧は物体表面と異なる動きを持つため、動く霧の時間方向の画質は引き続き確認が必要です。粒子とHUDはDLAA後に描きます。Frame GenerationとDLSS 5は含みません。

ShadowMapTests.exe --dlssは対応GPUで実際のNGX評価、ON/OFF・再有効化、タイトル・ステージ03・クラゲ表示、D3D12検証を行います。結果と画像はruntime/captures/ShadowMapTestsへ出力します。

## Debug / Developmentの比較と計測

DebugではF10でデバッグUIを表示し、ImGuiの「Anti-aliasing comparison」でOFF、FXAA、DLAA、DLAA + FXAAを切り替えます。Developmentでは起動時に開くWebパネルの「アンチエイリアス比較」を使用します。計測値は読み取り専用です。

GPU描画時間はRenderer::Drawの区間、DLAA時間はEvaluateと粒子合成用コピーの区間、最終パス時間はトーンマッピングと有効時のFXAAの区間です。CPU、Present待ち、Draw以前の更新コマンドは描画区間に含めません。FXAA単体だけの時間ではありません。

切り替え後の32フレームをウォームアップとして除外し、各モードの平均とサンプル数を保存します。切り替え先の平均は新しい計測に置き換え、シーン切り替え時は全モードをリセットします。同じ場面・カメラで比較してください。カメラ移動中の平均は同条件の比較になりません。Releaseのゲーム本体では計測リソースを作りません。

GpuTimestampTimerは汎用のGPU区間計測です。フェンス完了後に読み戻し、計測のための追加GPU待ちは行いません。現在のDirectXCommon::PostDrawがGPU完了を待つ契約に従います。非同期フレーム方式へ変更する場合はフレームごとの読み戻しバッファとフェンスを用意してください。

Development/DebugのShadowMapTests.exe --dlssは4モードを64フレームずつ実行し、プリセット状態、正のGPU時間、OFF時のDLAA時間リセット、D3D12検証を確認します。DevelopmentではWebパネルの操作経路と平均のウォームアップも検証します。aa-gpu-times.csvは各モードの最終フレームの実測値で、平均値ではありません。

## 低解像度描画

ゲームの3D入力を低解像度で生成し、表示解像度へ復元する機能を追加。低解像度化とTAAは独立したON/OFF設定。既定は低解像度OFF。操作・API・コストは [RenderResolution.md](../Renderer/RenderResolution.md)。

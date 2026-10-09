# DXR基盤

`Renderer` が `DxrRenderer` を所有し、既存の通常描画にDXRの検証経路を追加します。初期状態はOFFです。このクラスは交差判定と材質参照の基盤を担当し、太陽光のRTシャドウは `DxrShadowRenderer`、局所ライトのRTシャドウは `DxrLocalShadowRenderer`、RT反射は `DxrReflectionRenderer`、RT間接光は `DxrGlobalIlluminationRenderer` に実装しています。操作と負荷は [RTシャドウ](RTShadows.md)、[局所ライトRTシャドウ](RTLocalShadows.md)、[RT反射](RTReflections.md)、[RT間接光](RTGlobalIllumination.md) を参照してください。

## 操作

時間蓄積の画素ごとの判定、ノイズ量によるフィルター調整、実割当とGPU検証結果は [RTの履歴判定とノイズ除去](RTHistory.md) を参照してください。

- Debug：ImGuiの `DXR Foundation` で `Enabled` と `Show ray tracing output` をON。
- Development：開発Webパネルの「DXR基盤」で有効化し、検証画像の表示をON。
- 出力：法線、基本色（テクスチャ×物体色）、配置ID。検証画像は通常の最終描画後、HUD前に表示します。
- API：`Renderer::GetDxrRenderer()` → `GetSettings()` / `SetSettings()`。

非対応GPUでは加速構造・出力画像を生成せず、通常描画を継続します。リソース・パイプライン作成の例外はログへ記録し、DXRをOFFにします。

## 対象と構成

通常の `Object3d::Draw()` で提出された、標準頂点シェーダーを使う不透明・アルファ切り抜きモデルを登録します。加えて、ブレンドなしの `SkinningObject3d` はGPUスキニング済みの頂点を登録します。透明ブレンド、切り抜きOFFで物体色alphaが1未満、独自の頂点シェーダー変形は対象外です。`SetAlphaCutoff` を使った切り抜きではAnyHitで透明部分を通過します。設定と負荷は [アルファ切り抜き](AlphaMasks.md) を参照してください。光線は三角形の両面に命中します。カメラ外の物体もDrawが呼ばれていれば登録されますが、CPU側でDraw自体を省略した物体は登録されません。将来の影・反射では描画の可視性から独立した登録が必要です。

静止モデルはモデルごとにBLASを共有し、複数プリミティブ、インデックスあり／なしの三角形に対応します。頂点バッファ・インデックスバッファを保持するため、モデル削除後にGPUが解放済みバッファを参照しません。GPUバッファの置き換えと要素数変更を検出してBLASを再構築します。通常の `Queue` では同一バッファの頂点内容だけを変更する操作は対象外です。

変形モデルは `QueueDeformed` を使用します。オブジェクトIDとモデルの組み合わせごとにBLASを保持するため、同じモデルでも別の姿勢を混ぜません。初回は `ALLOW_UPDATE | PREFER_FAST_BUILD` で構築し、頂点revisionの変更時は `PERFORM_UPDATE` で既存の結果・scratchを再利用します。詳細と負荷は [変形モデルのRT対応](DeformedGeometry.md) を参照してください。

TLASは現在フレームに提出された配置から毎フレーム再構築します。位置・回転・拡大縮小、物体削除、シーン切り替えが反映され、特異行列や非有限値は登録しません。エンジンの行ベクトル行列をDXRの3×4配置行列へ転置して渡します。

DXR Tier 1.0以上を対象に `ID3D12Device5` / `ID3D12GraphicsCommandList4`、`lib_6_3`、State Object、ローカルルート、シェーダーテーブル、`DispatchRays` を使用します。既存のモデル用頂点／インデックスバッファとテクスチャを参照し、UV、補間法線、物体材質を取得します。法線は逆変換でワールド空間へ変換します。

## 同期と寿命

`BeginFrame` → 通常描画中の `Queue` → `EndFrame` → 必要なら `DrawDebug` → フェンス完了後の `ReadCompleted` の順です。`EndFrame(camera, false)` はシーンの構築だけを行い、検証用の光線探索を省きます。Rendererは検証画像を表示する場合だけ探索します。`HasValidScene()` と `GetSceneGpuAddress()` から、そのフレームのTLASをRTシャドウなどへ渡せます。`GetStatistics().hasValidFrame` は検証画像の有効性です。`Queue` の `shouldCastShadow` はデバッグ用とは別のインスタンスマスクへ反映し、Object3dでは既存のCastShadow設定を使用します。

`BeginFrame` は前フレームのGPU完了後に呼ぶ必要があります。既存の `DirectXCommon::PostDraw()` は毎フレームフェンスを待つため、その方式に合わせています。将来フレームを並列化するときはアップロード・TLAS・一時リソースをフレームごとに分離してください。

BLAS構築後、TLAS構築後にUAVバリアを入れます。出力はSRV→UAV→SRVへ遷移します。既存のモデルバッファ・テクスチャはGENERIC_READであり、光線シェーダーから読めます。未命中は暗い背景色です。空のシーンや無効入力では有効出力を公開せず、古い検証画像を表示しません。

## 実行コスト

- BLAS：初回使用時／メッシュのGPUバッファ置換時に構築。静止した同一モデルは再利用。
- 変形BLAS：物体ごとに保持し、姿勢変更時にrefit。GPUスキニング結果を直接参照し、RT用の頂点コピーを追加しません。UIに変形BLAS数・更新数・バッファ容量を表示します。
- TLAS：毎フレーム構築。現段階ではrefitや容量再利用・圧縮は未実装。
- 材質・シェーダーテーブル：毎フレームCPU準備とアップロード領域を確保。
- 出力：1280×720、RGBA16_FLOAT、1光線／ピクセル（921,600光線）。このGPUでの画像割当は7.5MiB。
- OFF：加速構造とフレーム用バッファを解放。初回ONで作成した出力・パイプラインは再有効化用に保持。
- 計測：BLAS/TLASのGPU構築とDispatchRaysを別々に表示。CPU準備・確保、検証画像の合成時間は含みません。バッファ容量はリソース幅の合計で、ドライバー内部の使用量・ヒープ割当の丸め・既存モデル・画像コピー用リソースは含みません。

## 検証

`Tests/Projects/DxrTests.vcxproj` をDebug x64でビルドし、作業ディレクトリを `project` として `generated/outputs/Debug/DxrTests.exe` を実行します。結果と比較用PNGは `runtime/captures/DxrTests` に出力します。

RTX 4060 Laptop GPU、1280×720、単純な三角形の検証で、初回構築0.166ms、キャッシュ利用時のTLAS構築0.045ms、光線探索0.254～0.256msを記録しました。少数フレームの動作検証値であり、実ゲームの性能保証ではありません。

命中・未命中、基本色、共有BLAS、重複登録の抑制、配置移動・削除、GPUメッシュ置換、非インデックス三角形、鏡映と拡縮、複数プリミティブと材質のシェーダーテーブル参照、配置ID、空のシーン、特異行列、OFF/ONを検証します。D3D12検証レイヤーのERROR/CORRUPTIONを失敗扱いにします。非対応環境ではフォールバックだけを検証し、光線探索のテストをSKIPとして記録します。

仕様：[Microsoft DXR Functional Spec](https://microsoft.github.io/DirectX-Specs/d3d/Raytracing.html)

# 変形モデルのRT対応

GPUスキニング後の頂点をDXRへ渡し、アニメーションの姿勢にRTシャドウと検証画像を追従させます。静止モデルは従来どおりモデル単位でBLASを共有し、変形モデルは物体単位でBLASを保持します。別々のキャラクターの姿勢を同じBLASへ上書きしません。

## 使用方法

1. DXR基盤とRTシャドウを有効にする。
2. 不透明なスキニングモデルの描画時に `SkinningObject3dManager::SetBlendMode(kBlendModeNone)` を使う。現在の既定ブレンドはNormalなので、明示的な指定が必要。
3. 遮蔽物は `SkinningObject3d::SetCastShadow(true)`、影を受ける面は `SetReceiveShadow(true)` を設定する。両方の初期値はfalse。
4. 少なくとも1回 `Update()` してGPU頂点を生成し、`SubmitRaytracing(renderer)` で提出する。画面内の通常描画は別途 `Draw()` で行う。未対応シーンでは従来のUpdate→Drawによる登録も使用できる。[RTシーン登録](RTSceneSubmission.md) を参照。

受け手の対応材質はStandardとToonです。通常の材質、法線マップ、環境・局所ライトを利用し、太陽光だけをRTの可視率で合成します。RT OFFでは元の描画へ戻ります。スキニングモデルのシャドウマップへの新規登録はこの工程に含みません。RT OFF時の既存の照明動作を維持します。

## BLAS更新と同期

初回の変形BLASは `ALLOW_UPDATE | PREFER_FAST_BUILD` で構築します。次回以降、同じ頂点・インデックスバッファと同じ要素数で頂点revisionが変わった場合は `PERFORM_UPDATE` でin-place更新します。結果バッファとscratchを再確保しません。バッファの置換、プリミティブ構成や要素数の変更はBLASを再構築します。

SkinningObject3dは前回の関節行列と現在の行列を比較します。姿勢が変わった場合だけ頂点revisionを進めるため、同じ姿勢のUpdateや停止中のDrawではBLASを再利用します。スキニングのDispatch自体は従来どおりUpdateで実行します。

GPUスキニング → UAVバリア → 頂点描画 → DXR用shader-read状態へ遷移 → BLAS/TLAS構築・必要なら検証DispatchRays → 元の頂点状態へ戻す → モーション描画・前フレーム頂点コピー → RTシャドウ、の順です。既存の毎フレームのフェンス待機を前提に、前フレーム完了後にリソースを再利用します。

変形する遮蔽物のrevisionを影の履歴判定へ含めます。姿勢変更時は現在の光線サンプルと履歴を画素ごとに照合し、不一致の画素を再開します。判定の制限とノイズ量による調整は [RTの履歴判定](RTHistory.md) を参照してください。

## 外部の変形処理から使うAPI

`DxrRenderer::QueueDeformed(objectId, model, world, material, vertices, geometryRevision, shouldCastShadow, vertexState)` を使います。

- verticesはVertexData形式で、モデルの全プリミティブの頂点をモデル順に連結したバッファ。インデックスは各プリミティブ内の相対インデックスを使用する。
- GPU書き込みとUAVバリアを完了し、呼び出し時のvertexStateを正しく渡す。現在はVERTEX_AND_CONSTANT_BUFFERまたはGENERIC_READを受け付ける。
- 内容を変更したらgeometryRevisionを進める。更新通知がなければ、同じアドレスの頂点変更を検出できない。
- refitで変更するのは頂点位置・法線・UV。インデックスの内容とプリミティブ順は固定とし、トポロジー変更ではバッファを置き換える。スキニングの姿勢検出は関節行列が対象で、同じバッファのウェイト・入力頂点を手動変更する操作には未対応。
- EndFrameまでバッファ内容を維持する。登録後はComPtrで寿命を保持するが、描画後に同一バッファへ別の姿勢を上書きしてはいけない。
- 同じobjectIdは1フレーム1回だけ登録する。画面内の描画を省略しても必要な遮蔽物は別途登録する。

現在は不透明・アルファ切り抜きの三角形が対象です。切り抜きは [アルファ切り抜き](AlphaMasks.md) を参照してください。透明ブレンド、独自頂点シェーダーの変形結果を自動取得する経路は未対応です。大きく姿勢が変わり続けるモデルではrefit後の探索性能が低下する場合があり、周期的な再構築やLODによる最適化は後工程です。[MicrosoftのDXR仕様](https://microsoft.github.io/DirectX-Specs/d3d/Raytracing.html) の更新制約に従います。

## 実行コスト

- 変形物体ごとにBLAS結果とscratchを保持する。静止モデルのように別の姿勢同士で共有できない。
- 姿勢変更時にBLAS refitを追加する。TLASは従来どおり毎フレーム構築する。
- GPUスキニング済み頂点を直接参照するので、RTのための頂点複製・追加スキニングDispatch・GPUからの読み戻しはない。
- 姿勢変更検出にはCPUで関節数×128bytesを比較し、変更時に保存する。前回の行列はCPUメモリに保持し、DXR OFFでも比較処理は行う。
- DXRパネルに変形BLAS数、更新数、変形BLASバッファ容量と実割当を追加した。構築GPU時間はrefitとTLASを含む区間で、CPU準備と既存スキニングDispatchは含まない。バッファ容量はresource幅、実割当はGetResourceAllocationInfoの合計でヒープの丸めを含む。既存のスキニング頂点、TLAS、ドライバー内部メモリは別。

## 検証

`DxrTests.exe` は別々の姿勢の分離、静止モデルとの共存、複数プリミティブの頂点オフセット・材質、インデックスあり／なし、refitによる境界の更新、停止中の再利用、削除、バッファ置換、入力不足・不正な状態、DXR OFFを検証します。

実際のGPUスキニングでも、アニメーション中の遮蔽物、CastShadow OFF、Standard/Toonの受け手、太陽光だけの合成、モーション頂点コピーとの同期、ノイズ除去の履歴、透明ブレンドの除外を検証します。D3D12検証レイヤーのERROR/CORRUPTIONを失敗扱いにします。

結果と画像は `runtime/captures/DxrTests/deformed-result.txt`、`skinning-result.txt`、`deformed-*.png`、`skinned-*.png` に出力します。

RTX 4060 Laptop GPUの小規模検証では、変形モデル2体のBLAS結果＋scratchのバッファ幅合計は7,168bytes、ヒープの丸めを含む実割当は262,144bytes（256KiB）でした。実スキニング検証は1体あたり8頂点・4三角形、別のバッファ検証は6頂点・2三角形です。refit1体とTLASを含む区間は検証実行間で約0.06～0.31msでした。小さい検証シーン・少数フレームでの値で、複雑なキャラクターや実ゲームの負荷を示す値ではありません。

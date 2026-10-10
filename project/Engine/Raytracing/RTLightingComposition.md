# 照明の合成

通常描画・RT命中先・RTGIの受け手で、拡散光と鏡面反射の配分を共有します。既存の照明キャプチャを使い、RTで求めた光を対応する環境光と置き換えます。太陽光・局所ライトの影は従来どおり各直接光に適用し、間接光全体に影を掛けません。

## 材質の配分

`LightingEnergy.hlsli` の `SurfaceFresnel` と `SurfaceDiffuseWeight` を使います。基本反射率は非金属0.04、金属は基本色です。拡散光の係数は `(1 - Fresnel * saturate(specularStrength)) * (1 - metallic)` です。鏡面強度は照明計算で0～1へ制限し、従来の2までの設定値を保持しても1を超える反射増幅には使いません。

Standardの太陽光、ポイント・スポット、半球環境光、IBL、RTGIに配分を適用します。直接光の鏡面をshininessで無効にした場合は、その直接光のフレネルによる減衰も省きますが、金属の拡散光は0のままです。粗さは既存のGGX分布と環境反射のMip・減衰を通じて鏡面へ作用します。Toon・ShadowToonにも拡散配分を適用し、既存の色の帯・ハイライト・リムは保持します。

StandardでIBLの拡散係数が正なら、半球環境光を追加せずIBLを使います。IBLの鏡面係数と材質の鏡面強度が正なら、従来のenableEnvironmentMapによる反射追加を省きます。各IBL成分が無効なら半球環境光・従来の環境反射を使用できます。UnlitやToonの従来環境反射は維持します。

## RTGIの置き換え

RTGIのraw・履歴・フィルター画像のRGBは従来どおり入射放射輝度の平均、alphaは距離で重み付けした命中サンプルの置き換え率です。黒い面への命中も数えるため、ゼロ輝度の命中とミスを区別します。alphaもRGBと同じ時間蓄積・空間フィルター・拡大の重みで処理します。

既存のindirectキャプチャから既存のreflectionEnvironmentキャプチャを引き、環境の拡散光だけを求めます。合成は `scene + strength * (rtDiffuse - capturedDiffuse * weightedCoverage)` です。rtDiffuseはrawのRGBに受け手の基本色と拡散配分を掛けた値です。

距離重み1の全命中では環境拡散光をRTGIへ置き換え、全ミスでは元の照明を保持します。一部命中・距離フェードでは残りの環境拡散光を保持します。strengthは0～1の置き換え率です。環境の鏡面反射と直接光は保持します。デバッグ表示は差分ではなくrtDiffuseを表示します。

低レベルの `DxrGlobalIlluminationInputs` は、既存入力に加えて `indirectTexture` と `indirectSrv` を必要とします。OffscreenRendererの `GetIndirectTexture()` と `GetIndirectSrvHandleGPU()` を渡します。欠けた入力・サイズ不一致では元の画像を返し、履歴をリセットします。画像は合成時にPIXEL_SHADER_RESOURCE状態で使用します。Rendererはこの入力を設定済みです。

## RT反射と画面空間の処理

RT反射は従来の `scene + strength * (rtSpecular - capturedSpecular * replacementCoverage)` を維持します。全ミスは環境反射、一部命中はミス分の環境反射を保持します。フレネルと鏡面強度の計算を材質側と共有します。粗い反射ではGGX VNDFと方向ごとの重みを使い、置き換え率も反射の重みから求めます。[粗いRT反射の推定](RTRoughReflections.md) を参照してください。

Rendererでは、有効なRTGIフレームでSSGIを省く既存処理に加え、有効なRT反射フレームではSSRを省きます。RTが無効・入力不足などで有効な出力を持たない場合は、既存の画面空間処理をユーザーの設定どおり使用します。未対応画素だけをSSGI・SSRで補う処理はありません。

## 実行コストと検証

追加レイ・追加画像・追加材質レイアウトはありません。RTGIの合成に既存indirect画像の読み取り1回を追加し、共通フィルタールートにSRV入力1個を追加します。CPUは入力の存在と画像サイズを確認します。材質の拡散配分にフレネル近似を追加します。GPU時間は材質・描画面積に依存し、通常実行の追加msは未測定です。

`DxrTests.exe --lighting` はRTGIと材質比較を実行します。強度0、黒い命中、一部命中、全ミス、環境鏡面の保持、金属の拡散光抑制、IBLと半球環境光・従来反射の非重複、通常描画とRTの一致を検証します。引数なしは影・反射・スキニング等も含む全DXRテストです。D3D12のERROR/CORRUPTIONを失敗扱いにします。

2026-10-09にRTX 4060 Laptop GPUで全DXRテストが終了コード0で成功しました。Developmentビルドとシェーダーのコンパイルも成功しています。比較用PNGと各テストの結果は `runtime/captures/DxrTests/` に保存します。D3D12のGPU検証を有効にした機能検証で、通常実行時の性能測定ではありません。

## 制限

これはリアルタイム用の光の配分・置き換えです。拡散直接光の既存の強度規約、Toonの演出的な帯・ハイライト、粗い反射の推定、旧環境反射の係数を保持するため、厳密な積分でエネルギー保存を保証するパストレーサーではありません。RTGIは1回の探索で、命中先の環境照明も含む近似です。レイの距離制限によるミスは環境光へ戻ります。レイ幅によるMip選択は [RTテクスチャのMip選択](RTTextureMips.md) に実装しています。追加1回の反射は [追加1回のRT反射](RTMultipleReflections.md) に実装しています。AO・二次光線の霧・2回を超える反射は別工程です。


RTGIの距離制限では、[GIのサンプル配分と探索範囲](RTDiffuseQuality.md) の距離重みを放射輝度と置き換え率の両方へ掛けます。距離フェード分は既存の環境拡散光を残します。

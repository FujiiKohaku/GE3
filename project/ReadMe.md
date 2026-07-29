# ポストエフェクト（PostEffect）課題提出資料 - ReadMe

## 1. 概要・提出者情報

| 項目 | 内容 |
|---|---|
| **作品名** | KohakuEngine 3Dレールシューティング |
| **学校名** | 日本工学院専門学校 デザインカレッジ |
| **学籍番号** | [学籍番号を入力してください] |
| **氏名** | 藤井 琥白 |
| **開発環境** | Windows 10/11, DirectX 12, C++20, HLSL (Shader Model 6.0) |

---

## 2. 必須内容（61点）

###  Grayscale（グレースケール描画）
- **HLSLファイル**: [`resources/Shaders/PostEffect/GrayScale.PS.hlsl`](file:///c:/Projects/KohakuEngine/project/resources/Shaders/PostEffect/GrayScale.PS.hlsl)
- **ゲーム内での組み込み・利用方法**:
  - **ポーズ画面（TABキー押下時）** にてゲームプレイ処理を一時停止する際、背景画面を白黒・モノクロ表示化するために適用。
  - 単なるモノクロ化にとどまらず、ガウスぼかし（`GaussianFilter`）およびSF風ホログラム走査線（`CyberScanline`）と**トリプルスタック合成**することで、ポーズメニュー時の視認性と演出デザイン性を高めています。

---

## 3. 加点要素対応一覧表

すべての指定加点要素をゲーム内に組み込み済みです。

| 加点項目 | 最高点 | 実装ファイル | ゲーム内での組み込み・演出用途 |
|---|:---:|---|---|
| **Vignetting** | 3点 | [`Vignette.PS.hlsl`](file:///c:/Projects/KohakuEngine/project/resources/Shaders/PostEffect/Vignette.PS.hlsl) | ボス撃破時の演出および被弾ダメージ時に、画面外周部を暗く落とし込んで臨場感・危機感を強調。 |
| **BoxFilter** | 3点 | [`BoxFilter.PS.hlsl`](file:///c:/Projects/KohakuEngine/project/resources/Shaders/PostEffect/BoxFilter.PS.hlsl) | 3x3カーネルによる画像平滑化フィルター。マルチパス描画および画面平坦化演出に使用。 |
| **GaussianFilter** | 5点 | [`GaussianFilter.PS.hlsl`](file:///c:/Projects/KohakuEngine/project/resources/Shaders/PostEffect/GaussianFilter.PS.hlsl) | ポーズ画面（TABキー）表示時に背景画面へ被写界深度ぼかしを付与。また各種ブラー合成のベースとして活用。 |
| **LuminanceBasedOutline** | 5点 | [`LuminanceBasedOutline.PS.hlsl`](file:///c:/Projects/KohakuEngine/project/resources/Shaders/PostEffect/LuminanceBasedOutline.PS.hlsl) | **ボス戦中**に自動発動。Sobelフィルタを用いてボス敵や高輝度オブジェクトの輝度境界線を輪郭線として鮮明に強調描画。 |
| **DepthBasedOutline** | 8点 | [`DepthBasedOutline.PS.hlsl`](file:///c:/Projects/KohakuEngine/project/resources/Shaders/PostEffect/DepthBasedOutline.PS.hlsl) | **通常ゲームプレイ時のメインポストエフェクト**として常時適用。深度バッファ（Zバッファ）から空間幾何境界を検出し、セル調のアウトラインを描画。 |
| **Radial Blur** | 5点 | [`RadialBlur.PS.hlsl`](file:///c:/Projects/KohakuEngine/project/resources/Shaders/PostEffect/RadialBlur.PS.hlsl) | **自機ブースト移動中（Shiftキー保持時）**に発動。自機および消失点を中心とする放射状ブラーを適用し、超高速移動感を演出。 |
| **Dissolve** | 4点 | [`Dissolve.PS.hlsl`](file:///c:/Projects/KohakuEngine/project/resources/Shaders/PostEffect/Dissolve.PS.hlsl) | **ボス撃破クリア時**に発動。2秒間かけてボスおよび背景画面をノイズ状にディゾルブ消滅させてクリア画面へシームレスに遷移。 |
| **Random** | 4点 | [`Random.PS.hlsl`](file:///c:/Projects/KohakuEngine/project/resources/Shaders/PostEffect/Random.PS.hlsl) | **Lキー押下**による動的デバッグトグル。擬似乱数を用いたノイズ・グリッチエフェクト画面の切り替えに対応。 |
| **その他（独自拡張）** | 20点 | 下記セクション参照 | 20種類以上の独自PostEffectを自作・パイプライン化し、ゲームの各種アクション・演出へ動的適用。 |

---

## 4. その他（独自追加PostEffect・20点加点枠）

項目リストにない独自PostEffectを多数開発し、ゲームの演出強化に組み込んでいます。

| PostEffect名 | HLSLファイル | ゲーム内での用途・演出効果 |
|---|---|---|
| **CyberScanline** | [`CyberScanline.PS.hlsl`](file:///c:/Projects/KohakuEngine/project/resources/Shaders/PostEffect/CyberScanline.PS.hlsl) | ポーズ画面（TABキー）時にレトロ・SFホログラムの走査線を表示。 |
| **SonicBoom** | [`SonicBoom.PS.hlsl`](file:///c:/Projects/KohakuEngine/project/resources/Shaders/PostEffect/SonicBoom.PS.hlsl) | ブースト開始（Shift押下）の瞬間に発動。プレイヤーの3D座標を画面UVに変換し、自機を中心とする衝撃音波リングの空間歪みを発生。 |
| **FocusLine** | [`FocusLine.PS.hlsl`](file:///c:/Projects/KohakuEngine/project/resources/Shaders/PostEffect/FocusLine.PS.hlsl) | ブースト移動中にアニメ風の集中線を画面周辺に生成し、スピード感を強調。 |
| **ChromaticAberration** | [`ChromaticAberration.PS.hlsl`](file:///c:/Projects/KohakuEngine/project/resources/Shaders/PostEffect/ChromaticAberration.PS.hlsl) | ブースト時や強力な攻撃の被弾時にRGBの色ズレ（色収差）を発生させ、衝撃を表現。 |
| **Fog** | [`Fog/Fog.PS.hlsl`](file:///c:/Projects/KohakuEngine/project/resources/Shaders/PostEffect/Fog/Fog.PS.hlsl) | 奥行きに応じた環境フォグを適用し、3D空間の空気感と距離感を表現。 |
| **Bloom** | [`Bloom/Bloom.PS.hlsl`](file:///c:/Projects/KohakuEngine/project/resources/Shaders/PostEffect/Bloom/Bloom.PS.hlsl) | 高輝度部分を抽出してガウスブラーでぼかし、加算合成することで発光体を表現。 |
| **Shockwave** | [`Shockwave.PS.hlsl`](file:///c:/Projects/KohakuEngine/project/resources/Shaders/PostEffect/Shockwave.PS.hlsl) | 爆発や強力な攻撃発生時に画面を屈折・歪ませる衝撃波リング演出。 |
| **HeatHaze** | [`HeatHaze.PS.hlsl`](file:///c:/Projects/KohakuEngine/project/resources/Shaders/PostEffect/HeatHaze.PS.hlsl) | エンジン噴射口や爆発の熱気による画面ゆらぎ（陽炎）をシミュレート。 |
| **GlassCrack** | [`GlassCrack.PS.hlsl`](file:///c:/Projects/KohakuEngine/project/resources/Shaders/PostEffect/GlassCrack.PS.hlsl) | ピンチ時やガラス破損演出時、画面全体にひび割れパターンと屈折を適用。 |
| **HexShield** | [`HexShield.PS.hlsl`](file:///c:/Projects/KohakuEngine/project/resources/Shaders/PostEffect/HexShield.PS.hlsl) | バリア・シールド展開時に六角形（ヘキサゴン）グリッドのエネルギー波を表示。 |
| **RainDrops** | [`RainDrops.PS.hlsl`](file:///c:/Projects/KohakuEngine/project/resources/Shaders/PostEffect/RainDrops.PS.hlsl) | レンズに付着した水滴と流れる水滴による屈折効果を表現。 |
| **BlackHoleDistortion** | [`BlackHoleDistortion.PS.hlsl`](file:///c:/Projects/KohakuEngine/project/resources/Shaders/PostEffect/BlackHoleDistortion.PS.hlsl) | ブラックホール状の強力な空間引き込み歪みを生成。 |

---

## 5. ゲーム操作方法と確認手順

ゲームを起動後、以下の操作で各種ポストエフェクトの動作を確認できます。

| 操作キー | アクション | 発動するPostEffect演出 |
|---|---|---|
| **通常走行時** | - | `DepthBasedOutline` + `Bloom` （幾何学セル調アウトライン常時適用） |
| **Shift** (保持) | 加速ブースト | `SonicBoom`（始動時歪み） + `RadialBlur` + `FocusLine` + `ChromaticAberration` + `Fog` |
| **ボス戦進入** | 自動切り替え | `LuminanceBasedOutline` （ボス・高輝度輪郭強調） |
| **ボス撃破時** | 自動演出 | `Dissolve` + `Vignette` （2秒かけてノイズ状消滅・クリア画面遷移） |
| **TAB** | ポーズ切替 | `GrayScale` + `GaussianFilter` + `CyberScanline` （トリプル合成ポーズ画面） |
| **L** | デバッグ切替 | `Random` ノイズエフェクトのON/OFF |
| **V** | デバッグワープ | ボス出現ポイント（Z = 1450.0f）まで一瞬で移動 |

---

## 6. ポストエフェクト技術システム構成

- **マルチステージ・パイプライン構造**:
  - `PostEffectStage::BeforeParticle` / `AfterParticle` のステージ分離により、パーティクル描画前後の適切な順序でポストエフェクトを加算・乗算・置換可能。
- **動的パラメータ連動**:
  - 3D世界座標から画面UV座標への自動変換（`WorldToScreen`）により、プレイヤーの位置を中心とした衝撃波（`SonicBoom`）やブーストブラーの中心点がリアルタイムに追従。

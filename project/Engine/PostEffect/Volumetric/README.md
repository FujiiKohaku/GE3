# ボリューメトリックライト

`PostEffectManager` が `VolumetricLightRenderer` を所有します。既存の太陽光とシャドウマップを利用し、HDR画面へ光の散乱を合成します。初期設定は有効、強度0.35、密度0.003、距離240、方向性0.35、32サンプルです。

```cpp
#include "Engine/PostEffect/Volumetric/VolumetricLightRenderer.h"

auto* volume = postEffects->GetVolumetricLightRenderer();
volume->SetEnabled(true);
volume->SetLightColor({ 0.7f, 0.85f, 1.0f });
volume->SetLightIntensity(0.35f);
volume->SetFogDensity(0.003f);
volume->SetMaxDistance(240.0f);
volume->SetAnisotropy(0.35f);
volume->SetSampleCount(32);
```

`SetLightDirection()` は既存の太陽光の向きを変更します。影も同じ方向へ更新する必要があるため、ゲーム更新中に呼び出してください。新しい向きで影を描くまで効果は無効になります。色は太陽光の色に乗算する追加の色調、強度は太陽光の強度に乗算する係数です。

開発用Web画面のポストエフェクト欄とImGuiの `Volumetric Light` からも調整できます。従来の `PostEffectType::VolumetricLight` は画面上の光を広げる別の効果で、この新しい機能は `GetVolumetricLightRenderer()` から設定します。太陽光の方向は既存の照明設定から変更できます。

## 描画順序と負荷

1. 既存のシャドウマップを描画する。
2. `Renderer` が `SetFrameInputs(camera, shadowRenderer)` を毎フレーム呼ぶ。
3. 3Dシーンの深度を使い、縦横1/2の解像度で光を計算する。
4. 深度を考慮してHDR画面へ合成し、既存のポストエフェクト・パーティクル・ブルーム・トーンマッピングへ渡す。HUDは最後に描く。

1280×720なら計算対象は640×360です。32サンプルでは約737万回の影参照（比較サンプラーによるフィルタリング込みの負荷はGPU依存）が発生します。追加描画は計算と合成の2回、追加画像は散乱光＋深度が約1.76MiB、透過率が約0.44MiBです。OFF時や必要な情報が無効な場合、これらの描画を省略します。

## 安全対策

- 影の更新・描画開始で完了フラグを無効化し、描画終了後だけ参照する。
- カメラなし、深度の準備不足、不正な行列は効果を省略する。影が無効・未生成の場合、局所霧は環境色と減衰を描き、光の散乱だけを省略する。
- 深度の復元はカメラ相対座標で行い、投影行列とカメラの基底を個別に検証する。遠いカメラ位置での計算誤差によるON/OFF切り替えを防ぐ。
- 入力は1回の描画で消費し、更新されない入力を再利用しない。
- 光源とシャドウマップの方向が一致しない場合は光の散乱を省略する。
- 影の対応範囲外では光を0にし、境界付近はフェードする。
- 低解像度の深度は手前側を採用し、合成時も深度差でフィルタリングする。
- セッターはNaN・無限値を安全な値へ置き換え、密度・距離・サンプル数などを制限する。
- 専用リソースやパイプラインの作成に失敗した場合はログを残し、通常の画面描画を続ける。

## 局所霧の設定

新しい描画クラスは追加せず、同じ `VolumetricLightRenderer` を拡張しています。設定構造体 `FogVolumeSettings` と形状列挙型 `FogVolumeShape` を使います。

```cpp
volume->SetLocalFogEnabled(true);
volume->SetFogColor({ 0.58f, 0.80f, 0.96f });
volume->SetNoiseParameters(0.025f, 0.35f, { 0.8f, 0.0f, 0.3f });

FogVolumeSettings fog;
fog.isEnabled = true;
fog.shape = FogVolumeShape::Sphere;
fog.center = { 0.0f, 4.0f, 200.0f };
fog.radius = 40.0f;
fog.density = 0.006f;
fog.edgeSoftness = 8.0f;
volume->SetFogVolume(0, fog);
```

箱を使う場合は `shape = FogVolumeShape::Box` とし、`halfExtents` に各軸の半幅を指定します。例えば `{ 75.0f, 12.0f, 75.0f }` は幅150・高さ24・奥行き150です。球も箱も外側では密度0、境界から内側の `edgeSoftness` 区間で滑らかに濃くなります。境界幅は半径または箱の最小半幅以下である必要があります。Setterの戻り値がfalseなら設定は変更されません。

最大8個を配置でき、重なった密度は加算して0.1に制限します。個々の濃度は0～0.05です。`SetFogVolume` は設定をコピーするため、呼び出し元の構造体を保持する必要はありません。無効化は `isEnabled = false` の設定を同じインデックスへ渡します。`ClearFogVolumes()` は配置だけ、`ResetLocalFog()` は色・高さ・ノイズ・有効状態も初期値へ戻します。

`SetHeightFog(baseHeight, density, heightFalloff)` で基準高さ以下に一定濃度、その上に指数的に薄れる霧を配置できます。濃度0で無効です。局所霧のモードでは従来の均一密度 `SetFogDensity()` を使用せず、配置した霧と高さ霧の密度を使用します。`SetLocalFogEnabled(false)` で従来の光だけの動作へ戻ります。

ノイズの強度は0～1、スケールは大きいほど細かくなります。移動速度はワールド単位／秒です。強度0で計算を省略します。霧のない場所へノイズだけで霧を追加せず、空間に固定した補間ノイズを動かします。

開発Webパネルの「立体霧」「局所霧 0～7」から各設定を操作できます。stage03では局所霧0をクラゲボスの足元へ配置し、X/Zだけ毎フレーム追従します。そのため局所霧0の中心座標はゲーム更新で上書きされます。手動配置には局所霧1～7を使用してください。`Game` がレンダラーを `SceneManager` へ非所有参照として渡し、次のシーンを初期化する前に `ResetLocalFog()` を呼びます。遅れて終了する旧シーンから新シーンの設定は消しません。

合成は `scene * transmittance + fogColor * (1 - transmittance) + scatteredLight` です。深度を使うため不透明物の手前まで積分し、UIは影響を受けません。後から描く透明パーティクルにはこの霧による減衰を個別適用していません。通常の距離フォグを同時に強く設定すると濃さが重なるので、遠景向けに調整してください。

太陽光は1つ、箱は軸に沿う形状です。スポットライト、多重散乱、時間方向の再投影は未対応です。サンプル間隔より薄い霧は縞や消失が出る場合があるため、広く薄い霧を基本とします。

## 検証と測定

`ShadowMapTests.vcxproj` のRelease x64で、球・箱・高さ霧、カメラが内部に入った場合、遠い霧が画面に影響しないこと、静止ノイズのフレーム一致、影なしの霧、入力拒否・設定リセット、D3D12デバッグ検証を確認しています。`--stage` は実際のstage03とクラゲボスを描画します。

テスト環境の測定値（1280×720、半解像度、32サンプル、デバッグレイヤー有効、ウォームアップ後20回平均）：

| 設定 | 合成を含む測定区間 | OFFとの差 |
|---|---:|---:|
| OFF | 0.046ms | — |
| 従来の光のみ | 0.243ms | 0.198ms |
| 局所霧2個、ノイズなし | 0.241ms | 0.196ms |
| 局所霧8個＋ノイズ | 0.640ms | 0.595ms |

GPUやシーンで変わる測定値です。低負荷にする場合は `SetSampleCount(16)`、霧2個以内、ノイズ強度0を目安にしてください。測定結果と比較画像は `captures/ShadowMapTests` へ出力します。

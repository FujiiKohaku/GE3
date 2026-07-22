# KohakuEngine

## TestScene Player Movement Effects

TestSceneのプレイヤー動作に、次のGPU Particleを追加している。

- `FootstepDust`: 歩行中、左右の足ボーンから交互に土埃を発生
- `DashDust`: ダッシュ中、左右の足ボーンから強い土埃を発生
- `LandingDust`: ジャンプの接地判定時に広い土埃を発生
- `BackflipDust`: 待機中のバク転開始時と着地時に土埃を発生
- `BackflipTrail`: バク転中、胸ボーンを追跡する専用Trailを発生
- `BodySpeedLines`: ダッシュ中、進行方向と逆向きに流れる速度線を発生

歩行・ダッシュ・バク転の発生時刻は、モデルと同じ階層の
`AnimationEvents/Robot_Walk.json`、`Robot_Dash.json`、`Robot_CombatIdle.json`で調整できる。
土埃の量・寿命・色・大きさ、Trailの幅と寿命、速度線の密度は各Effectの`Effect.json`で調整できる。

DirectX 12とC++20で開発している3Dゲームエンジンです。描画、GPUパーティクル、ポストエフェクト、モデル・テクスチャ管理、シーン管理、デバッグ機能をまとめています。

## 動作環境

| 項目 | 内容 |
|---|---|
| OS | Windows 10 / 11 64bit |
| 開発環境 | Visual Studio 2022以降、C++20 |
| Graphics API | DirectX 12 |
| Shader Model | 6.0 |
| 入力 | DirectInput 8 |
| 音声 | XAudio2 / Media Foundation |

## プロジェクト構造

```text
MyEngine/
├─ README.md
├─ generated/                     # ビルド生成物
└─ project/
   ├─ App/
   │  ├─ Game/                    # Player、Enemy、Bullet、Bossなど
   │  └─ Scene/
   │     ├─ Game.cpp              # エンジン全体の初期化と終了
   │     ├─ SceneManager.*        # シーン切り替え
   │     ├─ TitleScene.*          # タイトル
   │     ├─ LoadingScene.*        # エフェクトの段階的ウォームアップ
   │     ├─ GamePlayScene.*       # ゲーム本編
   │     ├─ ClearScene.*
   │     └─ GameOverScene.*
   ├─ Engine/
   │  ├─ DirectXCommon/           # Device、CommandList、Fence、DXIL読み込み
   │  ├─ Renderer/                # 描画フローの統括
   │  ├─ TextureManager/          # テクスチャ読み込みとGPU一括転送
   │  ├─ Effect/                  # Compute ShaderベースのGPUエフェクト
   │  ├─ Particle/                # パーティクル描画とメッシュ
   │  ├─ PostEffect/              # Bloom、Fogなど
   │  ├─ 2D/ 3D/ Camera/ Light/   # 基本描画機能
   │  └─ Debug/ Logger/           # 計測とログ
   ├─ resources/
   │  ├─ Shaders/                 # 通常シェーダーのHLSL
   │  ├─ Effects/                 # エフェクト単位のHLSLとEffect.json
   │  ├─ CompiledShaders/         # ビルド時に生成されるDXIL
   │  ├─ Textures/
   │  ├─ Models/
   │  └─ Sounds/
   ├─ tools/
   │  └─ CompileShaders.ps1       # HLSLの差分コンパイル
   └─ logs/                       # 実行ログとシェーダーエラー
```

## 起動とシーン遷移

```text
アプリケーション起動
  ↓
Game::Initialize
  ├─ DirectX 12と各Managerを初期化
  ├─ EffectManagerを一度だけ初期化
  └─ TitleSceneを予約
  ↓
TitleSceneを表示
  ↓ SPACE
LoadingSceneを表示
  ├─ 最初にロード画面を1フレーム描画
  ├─ 1フレームにつき1エフェクトをウォームアップ
  └─ 全22種類の完了を確認
  ↓
GamePlaySceneへ移動
```

`EffectManager`はゲーム起動時に一度だけ初期化し、ゲーム終了時にだけ破棄します。シーン切り替え時は再生中エフェクトのみ停止し、シェーダーやPSOなどの共通リソースは再利用します。

## エフェクトのウォームアップ

以前は`EffectManager::Initialize()`内で22種類を一括処理していたため、最初の画面が表示されるまで約5.5秒ブロックしていました。現在は`LoadingScene`で段階的に処理します。

| API | 役割 |
|---|---|
| `BeginWarmUp()` | 登録済みエフェクトを処理対象へ追加 |
| `UpdateWarmUp()` | 次の1種類だけGPU準備を実行 |
| `GetWarmUpProgress()` | 進捗を0.0～1.0で取得 |
| `IsWarmUpComplete()` | 全種類の完了確認 |

未完了のままゲームへ進まないため、ゲーム中の初回発生時に大きく停止する危険を抑えています。失敗時は`[EffectWarmUp] Failed: エフェクト名`をログへ記録します。

## シェーダーのビルドと読み込み

HLSLは実行時にコンパイルしません。Visual Studioのビルド前に`tools/CompileShaders.ps1`が再帰的に検索し、変更されたファイルだけDXILへ変換します。

```text
resources/Shaders/**/*.hlsl ─┐
resources/Effects/**/*.hlsl ─┼─ ビルド時にdxc.exeで変換
共有される*.hlsli ──────────┘
                 ↓
resources/CompiledShaders/**/*.dxil
                 ↓
DirectXCommon::LoadCompiledShader(HLSLのパス)
```

- `*.VS.hlsl` → `vs_6_0`
- `*.PS.hlsl` → `ps_6_0`
- `*.CS.hlsl` → `cs_6_0`
- `*.GS.hlsl` → `gs_6_0`
- HLSLまたは共有`.hlsli`が新しければ自動再コンパイル
- DebugとReleaseで同じDXIL出力先を使用
- ビルドエラーは`project/logs/ShaderCompile.log`へ出力

コード側はHLSLパスのまま登録できます。

```cpp
auto vertexShader = dxCommon_->LoadCompiledShader(
    L"resources/Shaders/Object3D/Object3d.VS.hlsl");
```

## テクスチャのGPU転送

`LoadTexture()`はアップロード命令をため、各テクスチャでGPU完了待ちを行いません。`Renderer::Draw()`の先頭で`TextureManager::FlushUploads()`を呼び、未転送分をまとめて送信してGPUを一度だけ待ちます。

```text
複数回のLoadTexture
  ↓ CPU側にアップロード命令を蓄積
Renderer::Draw
  ↓ FlushUploads
一括送信 → GPU待機1回 → 描画開始
```

呼び忘れを防ぐため、各シーンではなくRendererが一括転送を担当します。

## Debug計測ログ

Debugビルドでは`project/logs/EngineLog_日時.txt`へ以下を出力します。

| ログ | 計測内容 |
|---|---|
| `[StartupTime]` | WinMain開始から最初のPresent完了まで |
| `[EffectInit]` | EffectManager初期化の各区間と合計 |
| `[EffectPSO]` | エフェクトごとのCompute / Graphics PSO作成時間 |
| `[EffectWarmUp]` | 段階的ウォームアップの完了・失敗 |
| `[TextureUpload]` | 一括転送した枚数とGPU待機を含む時間 |

同じPC、同じDebugビルド、同じ起動条件で複数回計測してください。初回実行はOS・ドライバキャッシュの影響を受けるため、中央値で比較するのが安全です。

## 起動時間の改善結果

2026-07-14のDebugログによる同一環境での比較です。

| 指標 | Before | After | 改善 |
|---|---:|---:|---:|
| 最初の画面まで | 7,447 ms | 1,861 ms | 5,586 ms短縮、約75.0%減 |
| EffectManager初期化 | 5,991 ms | 520 ms | 5,471 ms短縮、約91.3%減 |
| 起動時の一括WarmUp | 5,472 ms | 0 ms | LoadingSceneへ移動 |

Afterの別実行は2,034 msでした。実装後の最初の画面は約1.9～2.0秒で表示されます。なおウォームアップ処理自体を削除したのではなく、安全なロード画面へ移動しています。

## ビルド

1. `project/KohakuEngine.sln`をVisual Studioで開く
2. `Debug | x64`などの構成を選択
3. ソリューションをビルド
4. HLSL差分コンパイル後、`generated/outputs/<構成>/KohakuEngine.exe`が生成される

シェーダー121件が更新済みの場合、ビルドログには次のように表示されます。

```text
Shader build completed. Compiled: 0, Up-to-date: 121
```

## 主な外部ライブラリ

- DirectXTex
- Assimp
- nlohmann/json
- Dear ImGui docking branch

## GPU Trail Particle

`EffectManager` に通常のメッシュパーティクルとは独立した、GPUベースのTrail描画を追加した。
`Effect.json` の `Render.Type` に `"Trail"` を指定すると、Compute Shaderが移動履歴を
`StructuredBuffer<TrailPoint>` に最大64点保存し、Vertex Shaderが隣接点をカメラ向きの帯状ポリゴンへ展開する。
`Render.Type` を省略した既存Effectは従来通り `Mesh` として処理される。

Trailでは次のJSONパラメータを指定できる。

- `MaxPoints`: 履歴点数（2～64）
- `LifeTime`: 各履歴点が残る時間
- `MinVertexDistance`: 新しい点を追加する最小移動距離
- `BreakDistance`: テレポートなどでTrailを切断する距離
- `StartWidth` / `EndWidth`: 先端と末端の幅
- `StartColor` / `EndColor`: 寿命に応じた色と透明度
- `TextureTiling`: Trail方向のテクスチャ反復数
- `FaceCamera`: カメラ方向を向く帯にするか
- `RootExtension`: Trailの根元を進行方向へ延長し、発生元へ重ねる距離

`MissileTrail` をこの方式へ移行済み。発生を停止した後も `LifeTime` の間だけ履歴が残り、
末端から透明になって消える。

## Multiple Point Lights

`LightManager` は最大32個のPoint Lightと最大8個のSpot Lightを同時にGPUへ渡せる。
各Collectionの0番は従来APIとの互換用として予約し、残りはハンドルで追加・更新・解放する。
Directional LightとAmbient Lightはそれぞれ1個。通常モデルとスキニングモデルの
Pixel Shaderは、有効なPoint LightとSpot Lightをすべて加算して描画する。

```cpp
PointLightHandle handle = LightManager::GetInstance()->AddPointLight(
    { 1.0f, 0.25f, 0.05f, 1.0f },
    { 0.0f, 2.0f, 0.0f },
    4.0f,
    8.0f,
    2.0f);

LightManager::GetInstance()->SetPointLightPosition(handle, newPosition);
LightManager::GetInstance()->RemovePointLight(handle);
```

動的ライトをまとめて破棄する場合は`ClearDynamicPointLights()`を使用する。
Spot Lightは`AddSpotLight()`、`UpdateSpotLight()`、`RemoveSpotLight()`で管理し、
一括破棄には`ClearDynamicSpotLights()`を使用する。

### Effect Light Assignment

Effectの`Light`セクションにPoint Lightを記述すると、再生時に動的ライトを自動取得する。
`FollowEmitter`が有効な場合はEmitter位置へ追従し、停止時は`FadeDuration`を使って減光した後、
Effect終了時にライトを自動解放する。ライト枠が満杯の場合もParticle本体は再生を継続する。

```json
"Light": {
  "Enabled": true,
  "Type": "Point",
  "Color": [1.0, 0.22, 0.03, 1.0],
  "Intensity": 5.0,
  "Radius": 8.0,
  "Decay": 2.0,
  "Offset": [0.0, 0.0, 0.0],
  "FollowEmitter": true,
  "FadeOut": true,
  "FadeDuration": 0.35
}
```

現在のJSONアサインはPoint Lightに対応している。`MissileTrail`はこの設定を使用している。

## GPU Particle Fields

Effectごとに最大16個のFieldをJSONから設定できる。通常のGPU ParticleとGPU Trailの両方に作用し、
複数Fieldの力は加算される。各Effect固有のUpdate Shaderを変更せず、共通のCompute Passで適用する。

- `Wind`: `Direction`の方向へ流す
- `Attractor`: `Position`へ引き寄せる
- `Repulsor`: `Position`から押し出す
- `Vortex`: `Direction`を回転軸として渦を作る
- `Space: "Local"`: Field位置がEffectのEmitterへ追従する
- `Space: "World"`: Field位置をワールド座標に固定する
- `Radius`: 影響半径
- `Strength`: 力の強さ
- `Falloff`: 中心から外側へ弱くなるカーブ。値が大きいほど外側で急に弱くなる

```json
"Fields": [
  {
    "Type": "Wind",
    "Space": "Local",
    "Position": [0.0, 1.5, 0.0],
    "Direction": [0.35, 1.0, 0.0],
    "Radius": 8.0,
    "Strength": 1.5,
    "Falloff": 0.6
  },
  {
    "Type": "Vortex",
    "Space": "World",
    "Position": [0.0, 3.0, 0.0],
    "Direction": [0.0, 1.0, 0.0],
    "Radius": 5.0,
    "Strength": 1.1,
    "Falloff": 1.0
  }
]
```

`Fields`自体を省略したEffectは、追加のField Compute Passを実行せず従来通り動作する。
`World`指定も現段階ではそのEffectが生成したParticleだけを対象とし、シーン内の全Effectへ作用する
グローバルFieldではない。`FieldDemo`にLocal WindとLocal Vortexの確認用設定を追加済み。

## Animation Events

glTF内のAnimation名と時刻を使い、Bone位置からEffectなどのゲーム処理を発生させる。
Debugビルドでモデルを初めてインポートすると、glTFと同じ階層の`AnimationEvents`フォルダへ
AnimationごとのJSONテンプレートを不足分だけ生成する。既存JSONは上書きしない。
Releaseビルドと通常の再生処理は生成済みJSONの読み込みだけを行う。

```text
precision_robot_rigged_single_gltf/
├─ precision_robot_rigged_single.gltf
└─ AnimationEvents/
   ├─ Robot_Punch.json
   ├─ Robot_Punch_L.json
   └─ Robot_RocketUppercut.json
```

```json
{
  "Animation": "Robot_Punch",
  "Duration": 1.5,
  "Events": [
    {
      "Time": 0.2,
      "Name": "PlayEffect",
      "Bone": "hand.R",
      "Value": "HandFlame"
    }
  ]
}
```

`PlayAnimation`は前フレームから現在フレームまでに通過したイベントをキューへ追加する。
Animationのループ境界、初回再生、ブレンド中の新しいAnimationに対応し、`deltaTime`が0の場合は
イベントを発生させない。利用側は`PopTriggeredEvent()`でイベントを順番に取得する。
TestSceneの3段パンチコンボは、右手・左手・右手のBoneに設定したJSONイベントから
`HandFlame`を発生させる。

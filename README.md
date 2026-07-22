# KohakuEngine

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

`LightManager` は最大16個のPoint Lightを同時にGPUへ渡せる。0番は従来の
`SetPointLight()` APIとの互換用として予約し、1～15番はハンドルで追加・更新・解放する。
通常モデルとスキニングモデルのPixel Shaderは、有効なPoint Lightをすべて加算して描画する。

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

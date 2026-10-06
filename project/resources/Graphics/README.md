# グラフィックのデータ設定

`visual-presets.json` は照明、大気、モデルと材質の対応、材質の光沢値、後処理の組み合わせ、エフェクト固有パラメータの既定値を保持します。アプリ側の `VisualPresetLibrary` が初回参照時に読み込み、以後はキャッシュから取得します。照明と大気の未登録キーは `default`、材質の未登録モデルは `defaultMaterial` を使います。エフェクト名は `GetPostEffectTypeName()` の名前に合わせます。不明なエフェクト名や不足した必須データは読み込みエラーです。

- `LightManager::ApplyLightingPreset()` はシーン名を受け取らず、設定データだけを適用します。
- `SceneManager::ApplyAtmospherePreset()` と `ApplyPostEffectChain()` は任意の設定・組み合わせを受け取ります。
- `RadialBlurSettings` はサンプル数、幅、演出強度を指定します。ブースト状態・入力キーの解釈はゲーム側で行います。
- `SetPostEffectParameters(type, Vector3)` は各エフェクトの3個の追加floatを指定します。意味はシェーダーが決め、未指定時はJSONの `effectParameters` を使います。CPU/HLSLのバッファ配置は変更していません。
- `Object3d::SetShadowMaterial()` は影用VS、両面描画、モデル座標系の範囲拡張を指定します。VSはPOSITION入力とb0のワールド行列、b1のライト行列、b2の4個のfloatという契約に従います。変形量の計算はオブジェクト所有側が行います。

影用パイプラインはシェーダーと両面設定ごとに初回使用時に生成して再利用します。毎フレームの描画回数・影解像度・GPUサンプル数は従来と同じです。JSONの読み込みとパイプライン生成は初回コスト、描画時はキャッシュ参照が加わります。

特定のシーンやボスの演出は、それを所有するシーン・キャラクターに残します。エンジン側はシーン名、ボス名、ブースト用のキーを判定しません。

# 登録式の開発パネル

`ENABLE_DEVELOPMENT_TOOLS`を有効にした構成で使用します。HTTP通信と画面生成はエンジンが担当し、ゲームの具体的なクラスを参照しません。

## Getter／Setterを直接登録する

シーンの初期化で次のように登録します。Setterは適用できた場合に`true`を返してください。

```cpp
DevelopmentWebPanel& panel = DevelopmentWebPanel::GetInstance();
panel.RegisterFloat(this, "moveSpeed", "移動速度", 0.0f, 10.0f,
    &MyScene::GetMoveSpeed, &MyScene::SetMoveSpeed);
panel.RegisterBool(this, "invincible", "無敵モード",
    &MyScene::IsInvincible, &MyScene::SetInvincible);
```

Getterはそれぞれ`float () const`と`bool () const`、Setterは`bool (float)`と`bool (bool)`です。登録元の変数へ直接書き込まず、登録したSetterを呼びます。数値の範囲・有限性・boolの形式はエンジン側でも検証します。

シーンの終了処理で、登録を解除します。

```cpp
DevelopmentWebPanel::GetInstance().UnregisterOwner(this);
```

登録対象は所有しません。Getter／Setterの呼出し、登録・解除はゲームのスレッドで行います。新しいシーンの登録は前シーンの登録と置き換わるため、遅れて実行される旧シーンの解除は新シーンへ影響しません。

## 複数項目・動的項目

`RegisterSource`で状態取得、項目定義、bool／数値Setter、コマンドをメンバー関数として登録できます。既存のシーンとポストエフェクトはこの方式で移行しています。項目定義は`key`、`label`、`type`を持つJSON配列です。

- `bool`：チェックボックス
- `number`：`minimum`、`maximum`、`step`を指定
- `select`：`options`に数値の`value`と表示名の`label`を指定
- `action`：引数なしの操作ボタン

`group`で画面内の分類、`valueKey`で状態側の別名を指定できます。動的な敵やオブジェクトの項目も、登録元が定義します。`isScene=false`の登録はシーン切替後も維持されます。必要ならシーン切替時に呼ぶメンバー関数も登録できます。

起動時にHTMLをキャッシュします。HTML変更を確認するときはパネルを再起動してください。`KOH_DEV_PANEL_NO_BROWSER`を設定すると、ブラウザを自動起動しません。

## 検証

`project/Tests/Projects/DevelopmentWebPanelTests.vcxproj`のRelease x64をビルドし、`project`を作業ディレクトリとして`generated/outputs/Release/DevelopmentWebPanelTests.exe`を実行します。実HTTP通信、送信詰まり、型・範囲検証、Setter呼出し、登録解除とシーン切替を検証します。

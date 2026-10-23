# プロジェクトのフォルダ構成

| フォルダ | 内容 |
| --- | --- |
| `App` | ゲーム固有のコード |
| `Engine` | エンジンのコード |
| `resources` | モデル・画像・音声・シェーダー |
| `externals` | 外部ライブラリ |
| `Tests` | テストとテスト用プロジェクト |
| `Tools` | 開発ツール |
| `Docs` | 開発資料 |
| `generated` | 実行時の出力とローカル設定。Git管理対象外 |

実行時の作業ディレクトリは `project` を使用します。

```text
generated/
├─ Captures/       GIF録画、モデルプレビュー、描画検証の画像
├─ Screenshots/    スクリーンショット
├─ logs/           エンジン・シェーダー・DLSSのログ
│  └─ legacy/      旧logsフォルダから移した過去のログ
├─ Dumps/          クラッシュダンプ
├─ config/         ImGuiのローカル設定
└─ settings/       カメラのローカル設定
```

`config` と `settings` にはユーザーの設定を保存します。ログや画像を整理するときも、設定を引き継ぐ場合はこの2つを保持します。

リポジトリ直下の `generated` は、ビルド成果物・検証ログの保存先です。ここで説明する実行時出力は `project/generated` に保存します。

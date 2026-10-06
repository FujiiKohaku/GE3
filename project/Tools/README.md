# 開発ツール

- `ModelPreview/`：別プロジェクトのモデルプレビュー。起動は `StartModelPreview.ps1`。
- `Scratch/`：手動実行する補助スクリプト。
- ビルド・シェーダー処理のスクリプトはこのフォルダに配置します。

ゲームとプレビューの作業ディレクトリは引き続き `project` です。実行時の画像、ログ、ダンプは `project/runtime` 配下に出力します。未使用の旧フォルダ・IDEバックアップ・旧ImGui一式は整理時に削除しました。Visual Studioが使用中の `.vs` は保持します。

使用中のImGuiは `project/externals/imgui` です。

テスト用プロジェクトは `project/Tests/Projects`、モデルプレビュー用プロジェクトは `project/Tools/ModelPreview/ModelPreview.vcxproj` に配置します。資料とスペルチェック辞書は `project/Docs`、ImGuiの個人設定は `project/runtime/config/imgui.ini` に保存します。

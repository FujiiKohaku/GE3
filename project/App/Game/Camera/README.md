# Gameplay camera settings
Open Pause -> Settings -> Graphics settings -> Camera settings. Up/down selects a row; left/right changes it.
Enter/A cycles a preset on row 1. Escape/B returns; row 4 also returns to Graphics settings.
Both Debug and Release use the game UI; ImGui is optional.

Presets (distance / vertical FOV degrees): original 35 / 25.7831; low flight/speed 27 / 32;
aircraft emphasis 22 / 28. Fine adjustment: distance 18-60 in steps of 1; FOV 20-60 degrees
in steps of 1. Manual adjustments are marked Custom. Boost widens FOV by the original 4/3
ratio, limited to 75 degrees. Both render and aim cameras use the new FOV.
Changes apply on resume and save to runtime/settings/camera.json. New stages, retries and
restarts load the saved values. Missing, malformed or out-of-range files use original values.
Save failure is shown in the camera settings hint; the current scene still uses the change.
The three presets are starting points for in-game comparison, not copies of other games.

Validation: Tests/CameraPreferencesTests.cpp with CameraPreferences.cpp and BasePauseState.cpp
checks preset cycling, range limits, save/load, malformed files and keyboard/gamepad-neutral
pause state actions. Build the main game in Release to verify UI integration.


映像設定: TAB → 設定 → 映像設定。上下で項目、左右または決定でAA/描画品質を切り替える。
AA: OFF / DLAA / FXAA / DLAA + FXAA / TAA / TAA + FXAA（DLAA非対応では該当項目をスキップ）。
描画品質: 高品質 / 標準 / 軽量。既存のRT・ボリューム描画品質プリセットを利用し、効果の有効/無効は変更しない。
カメラ設定は映像設定の3行目。描画設定は今回の起動中のみ保持する（カメラ設定は従来どおり保存）。

マウス操作: ポーズ中は自動でカーソルを表示。ボタン本体を左クリックで決定、調整項目は「−」「＋」でも変更できる。
カーソルを動かしてボタンに重ねると選択表示が変わる。ゲーム再開後はF2によるカーソル表示設定に戻る。
クリック判定は描画と同じ矩形を使い、実際のクライアント領域から1280×720のUI座標へ変換する。
ImGuiに依存せずDebug/Release共通で動作する。ESCはポーズ中は戻る操作として扱う。

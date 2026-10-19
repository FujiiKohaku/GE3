// ポーズ画面の状態ごとに、選択操作と決定・キャンセル時の動作を分ける。
#include "BasePauseState.h"

// 上下入力で選択位置を移動し、決定・キャンセルを現在のポーズ状態へ渡す。
void BasePauseState::Navigate(PauseStateContext& context, const PauseStateInput& input)
{
    if (input.up) context.selection = (context.selection + 3) % 4;
    if (input.down) context.selection = (context.selection + 1) % 4;
}

// ポーズのメニュー項目を表示し、続行・再挑戦・設定などの決定を処理する。
class PauseMenuState final : public BasePauseState {
public:
    PauseViewState GetType() const override { return PauseViewState::Menu; }
    void HandleInput(PauseStateContext& context, const PauseStateInput& input) const override {
        Navigate(context, input);
        if (input.back) { context.selection = 0; Activate(context); }
        else if (input.confirm) Activate(context);
        else if (input.settings) { context.selection = 2; Activate(context); }
        else if (input.retry) { context.selection = 1; Activate(context); }
        else if (input.title) { context.selection = 3; Activate(context); }
    }
    void Activate(PauseStateContext& context) const override {
        if (context.selection >= 0 && context.selection < 4 && context.menuActions[context.selection]) {
            context.menuActions[context.selection]();
        }
    }
    PauseDisplay GetDisplay(const PauseDisplayInput& input) const override {
        PauseDisplay display { "PAUSE MENU", {}, input.gamepad ? "十字キー上下: 選択 / A: 決定" : "↑↓: 選択 / ENTER: 決定" };
        display.rows = input.gamepad ? std::array<std::string, 4> {
            "ゲーム再開 [A / B / START]", "リトライ [A]", "設定 [A]", "タイトルに戻る [A]" } :
            std::array<std::string, 4> { "ゲーム再開 [TAB]", "リトライ [R]", "設定 [S / ENTER]", "タイトルに戻る [T]" };
        if (input.selection >= 0 && input.selection < 4) display.rows[input.selection].insert(0, "> ");
        return display;
    }
};

// ポーズ中の感度調整・上下反転などの設定操作を処理する。
class PauseSettingsState final : public BasePauseState {
public:
    PauseViewState GetType() const override { return PauseViewState::Settings; }
    void HandleInput(PauseStateContext& context, const PauseStateInput& input) const override {
        Navigate(context, input);
        if (input.left) AdjustSensitivity(context, -0.1f);
        if (input.right) AdjustSensitivity(context, 0.1f);
        if (input.back) context.changeState(PauseViewState::Menu, 2);
        else if (input.confirm) Activate(context);
    }
    void Activate(PauseStateContext& context) const override {
        if (context.selection == 1) context.togglePitch();
        if (context.selection == 3) context.changeState(PauseViewState::Graphics, 0);
    }
    void AdjustSensitivity(PauseStateContext& context, float amount) const override {
        if (context.selection == 0) context.adjustPad(amount);
        else if (context.selection == 2) context.adjustMouse(amount);
    }
    bool HasSensitivityControls(int row) const override { return row == 0 || row == 2; }
    void HandleControlToggle(PauseStateContext& context, bool triggered) const override {
        if (triggered) context.toggleControl();
    }
    PauseDisplay GetDisplay(const PauseDisplayInput& input) const override {
        PauseDisplay display { "設定", {}, input.gamepad ?
            "十字キー上下: 選択 / 左右: 感度\nA: 決定 / B: 戻る" :
            "↑↓: 選択 / ←→: 感度 / ENTER: 決定\nC: マウス操作切替 / ESC: 戻る" };
        display.hint += "\nGIF録画時間: " + std::to_string(input.gifRecordingSeconds) + "秒（5 / 10 / 15 / 30秒）";
        display.hint += "\nF7: 短く / F8: 長く / F9: 録画・停止";
        display.rows = { "PAD AIM: " + std::to_string(static_cast<int>(input.padSensitivity * 100.0f + 0.5f)) + "%",
            std::string("PAD PITCH: ") + (input.invertedPitch ? "INVERTED" : "NORMAL"),
            "MOUSE: " + std::to_string(static_cast<int>(input.mouseSensitivity * 100.0f + 0.5f)) + "%",
            input.gamepad ? "映像設定 [A]" : "映像設定 [ENTER]" };
        if (input.selection >= 0 && input.selection < 4) display.rows[input.selection].insert(0, "> ");
        return display;
    }
};

// Camera settings use the same four-row UI in Debug and Release.
class PauseCameraState final : public BasePauseState {
public:
    PauseViewState GetType() const override { return PauseViewState::Camera; }
    void HandleInput(PauseStateContext& context, const PauseStateInput& input) const override {
        Navigate(context, input);
        if (input.left) AdjustSensitivity(context, -1.0f);
        if (input.right) AdjustSensitivity(context, 1.0f);
        if (input.back) context.changeState(PauseViewState::Graphics, 2);
        else if (input.confirm) Activate(context);
    }
    void Activate(PauseStateContext& context) const override {
        if (context.selection == 0) context.cycleCameraPreset(1);
        if (context.selection == 3) context.changeState(PauseViewState::Graphics, 2);
    }
    void AdjustSensitivity(PauseStateContext& context, float amount) const override {
        const int direction = amount < 0 ? -1 : 1;
        if (context.selection == 0) context.cycleCameraPreset(direction);
        if (context.selection == 1) context.adjustCameraDistance(float(direction));
        if (context.selection == 2) context.adjustCameraFov(float(direction));
    }
    bool HasSensitivityControls(int row) const override { return row >= 0 && row <= 2; }
    PauseDisplay GetDisplay(const PauseDisplayInput& input) const override {
        PauseDisplay display { "カメラ設定", {}, input.gamepad ?
            "十字キー上下: 選択 / 左右: 変更\nA: プリセット切替 / B: 戻る" :
            "↑↓: 選択 / ←→: 変更\nENTER: プリセット切替 / ESC: 戻る" };
        display.hint += "\n距離: 18～60 / 縦画角: 20～60度";
        display.hint += input.cameraSaveSucceeded ? "\n自動保存・再開すると比較できます" : "\n保存失敗（今回のプレイには反映済み）";
        display.rows = { "プリセット: " + input.cameraPreset,
            "距離: " + std::to_string(int(input.cameraDistance + 0.5f)),
            "縦画角: " + std::to_string(int(input.cameraFovDegrees + 0.5f)) + "度",
            input.gamepad ? "戻る [A / B]" : "戻る [ENTER / ESC]" };
        if (input.selection >= 0 && input.selection < 4) display.rows[input.selection].insert(0, "> ");
        return display;
    }
};

class PauseGraphicsState final : public BasePauseState {
public:
    PauseViewState GetType() const override { return PauseViewState::Graphics; }
    void HandleInput(PauseStateContext& context, const PauseStateInput& input) const override {
        Navigate(context, input);
        if (input.left) AdjustSensitivity(context, -1);
        if (input.right) AdjustSensitivity(context, 1);
        if (input.back) context.changeState(PauseViewState::Settings, 3);
        else if (input.confirm) Activate(context);
    }
    void Activate(PauseStateContext& context) const override {
        if (context.selection < 2) AdjustSensitivity(context, 1);
        if (context.selection == 2) context.changeState(PauseViewState::Camera, 0);
        if (context.selection == 3) context.changeState(PauseViewState::Settings, 3);
    }
    void AdjustSensitivity(PauseStateContext& context, float amount) const override {
        const int direction = amount < 0 ? -1 : 1;
        if (context.selection == 0 && context.cycleAntiAliasing) context.cycleAntiAliasing(direction);
        if (context.selection == 1 && context.cycleRenderQuality) context.cycleRenderQuality(direction);
    }
    bool HasSensitivityControls(int row) const override { return row == 0 || row == 1; }
    PauseDisplay GetDisplay(const PauseDisplayInput& input) const override {
        PauseDisplay display { "映像設定", {}, input.gamepad ?
            "十字キー上下: 選択 / 左右: 変更\nA: 決定・切替 / B: 戻る" :
            "↑↓: 選択 / ←→: 変更\nENTER: 決定・切替 / ESC: 戻る" };
        display.rows = { "AA: " + input.antiAliasing, "描画品質: " + input.renderQuality, "カメラ設定", "戻る" };
        display.hint += "\n" + input.dlaaStatus + "\n変更は即時反映・今回の起動中に保持";
        if (input.selection >= 0 && input.selection < 4) display.rows[input.selection].insert(0, "> ");
        return display;
    }
};

// メニューか設定かに応じて、共有のポーズ状態オブジェクトを返す。
const BasePauseState& GetPauseState(PauseViewState state)
{
    static const PauseMenuState menu;
    static const PauseSettingsState settings;
    static const PauseCameraState camera;
    static const PauseGraphicsState graphics;
    if (state == PauseViewState::Graphics) return graphics;
    if (state == PauseViewState::Camera) return camera;
    return state == PauseViewState::Settings ? static_cast<const BasePauseState&>(settings) : static_cast<const BasePauseState&>(menu);
}

PauseButtonRect GetPauseRowRect(PauseViewState page, int row) {
    constexpr float centers[] = { 290, 360, 430, 570 };
    const float width = row == 2 || (row == 0 && (page == PauseViewState::Camera || page == PauseViewState::Graphics)) ? 300.0f : 220.0f;
    return {640 - width * 0.5f, centers[row] - 22, width, 44};
}
PauseButtonRect GetPauseAdjustmentRect(int row, int direction) {
    const auto button = GetPauseRowRect(PauseViewState::Menu, row);
    return { direction < 0 ? 458.0f : 790.0f, button.y, 32, 44 };
}
PauseMouseHit HitTestPauseButtons(const BasePauseState& page, float x, float y) {
    for (int row = 0; row < 4; ++row) {
        if (page.HasSensitivityControls(row)) {
            for (int direction : {-1, 1}) {
                if (GetPauseAdjustmentRect(row, direction).Contains(x, y)) return {row, direction};
            }
        }
        if (GetPauseRowRect(page.GetType(), row).Contains(x, y)) return {row, 0};
    }
    return {};
}

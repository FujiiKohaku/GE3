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
        if (context.selection == 3) context.changeState(PauseViewState::Menu, 2);
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
            "十字キー上下: 選択 / 左右: 感度\nA: 上下反転 / B: 戻る" :
            "↑↓: 選択 / ←→: 感度 / ENTER: 決定\nC: マウス操作切替 / ESC: 戻る" };
        display.hint += "\nGIF録画時間: " + std::to_string(input.gifRecordingSeconds) + "秒（5 / 10 / 15 / 30秒）";
        display.hint += "\nF7: 短く / F8: 長く / F9: 録画・停止";
        display.rows = { "PAD AIM: " + std::to_string(static_cast<int>(input.padSensitivity * 100.0f + 0.5f)) + "%",
            std::string("PAD PITCH: ") + (input.invertedPitch ? "INVERTED" : "NORMAL"),
            "MOUSE: " + std::to_string(static_cast<int>(input.mouseSensitivity * 100.0f + 0.5f)) + "%",
            input.gamepad ? "戻る [A / B]" : "戻る [ENTER / ESC]" };
        if (input.selection >= 0 && input.selection < 4) display.rows[input.selection].insert(0, "> ");
        return display;
    }
};

// メニューか設定かに応じて、共有のポーズ状態オブジェクトを返す。
const BasePauseState& GetPauseState(PauseViewState state)
{
    static const PauseMenuState menu;
    static const PauseSettingsState settings;
    return state == PauseViewState::Settings ? static_cast<const BasePauseState&>(settings) : static_cast<const BasePauseState&>(menu);
}

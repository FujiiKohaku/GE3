// ポーズ画面の状態ごとに、選択操作と決定・キャンセル時の動作を分ける。
#pragma once
#include <array>
#include <functional>
#include <string>

enum class PauseViewState { Menu, Settings, Camera, Graphics };
struct PauseStateInput {
    bool up = false, down = false, left = false, right = false;
    bool back = false, confirm = false;
    bool settings = false, retry = false, title = false;
};
struct PauseStateContext {
    int& selection;
    std::array<std::function<void()>, 4> menuActions;
    std::function<void(PauseViewState, int)> changeState;
    std::function<void(float)> adjustPad;
    std::function<void(float)> adjustMouse;
    std::function<void()> togglePitch;
    std::function<void()> toggleControl;
    std::function<void(int)> cycleCameraPreset;
    std::function<void(float)> adjustCameraDistance;
    std::function<void(float)> adjustCameraFov;
    std::function<void(int)> cycleAntiAliasing;
    std::function<void(int)> cycleRenderQuality;
};
struct PauseDisplayInput {
    int selection;
    bool gamepad;
    bool invertedPitch;
    float padSensitivity;
    float mouseSensitivity;
    int gifRecordingSeconds = 5;
    std::string cameraPreset = "標準";
    float cameraDistance = 35.0f;
    float cameraFovDegrees = 25.7831f;
    bool cameraSaveSucceeded = true;
    std::string antiAliasing = "OFF";
    std::string renderQuality = "カスタム";
    std::string dlaaStatus;
};
struct PauseDisplay {
    std::string title;
    std::array<std::string, 4> rows;
    std::string hint;
};

class BasePauseState {
public:
    virtual ~BasePauseState() = default;
    virtual PauseViewState GetType() const = 0;
    virtual void Enter(PauseStateContext& context) const { context.selection = 0; }
    virtual void Exit(PauseStateContext&) const {}
    virtual void HandleInput(PauseStateContext& context, const PauseStateInput& input) const = 0;
    virtual void Activate(PauseStateContext& context) const = 0;
    virtual void AdjustSensitivity(PauseStateContext&, float) const {}
    virtual bool HasSensitivityControls(int) const { return false; }
    virtual void HandleControlToggle(PauseStateContext&, bool) const {}
    virtual PauseDisplay GetDisplay(const PauseDisplayInput& input) const = 0;
protected:
    // 上下入力で選択位置を移動し、決定・キャンセルを現在のポーズ状態へ渡す。
    static void Navigate(PauseStateContext& context, const PauseStateInput& input);
};

// Shared by drawing and mouse picking in the 1280 x 720 UI coordinate system.
struct PauseButtonRect {
    float x, y, width, height;
    bool Contains(float px, float py) const {
        return px >= x && px < x + width && py >= y && py < y + height;
    }
};
struct PauseMouseHit { int row = -1; int adjustment = 0; };
PauseButtonRect GetPauseRowRect(PauseViewState page, int row);
PauseButtonRect GetPauseAdjustmentRect(int row, int direction);
PauseMouseHit HitTestPauseButtons(const BasePauseState& page, float x, float y);

// メニューか設定かに応じて、共有のポーズ状態オブジェクトを返す。
const BasePauseState& GetPauseState(PauseViewState state);

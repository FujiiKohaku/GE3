#include "App/Scene/Title/TitleScene/Settings/SettingsState.h"
#include "App/Scene/Title/TitleScene/Settings/SettingsTransitionState.h"
#include "App/Scene/Title/TitleScene/TitleScene.h"
#include "Engine/Input/Input.h"

// 設定画面の入力を受け付け、上下反転の切替や戻る操作を処理する。
TitleViewState SettingsState::GetType() const
{ return TitleViewState::Settings; }

void SettingsState::HandleInput(TitleScene& scene, const Input& input) const
{
    if (input.IsKeyTrigger(DIK_BACK) || input.IsKeyTrigger(DIK_S) ||
        input.IsGamepadButtonTrigger(XINPUT_GAMEPAD_B)) {
        scene.StartSettingsTransition(false);
        return;
    }
    if (input.IsGamepadButtonTrigger(XINPUT_GAMEPAD_A) ||
        input.IsKeyTrigger(DIK_P) ||
        input.IsGamepadButtonTrigger(XINPUT_GAMEPAD_DPAD_LEFT) ||
        input.IsGamepadButtonTrigger(XINPUT_GAMEPAD_DPAD_RIGHT)) {
        Input::GetInstance()->ToggleGamepadPitchInverted();
    }

}

void SettingsState::Update(TitleScene& scene, float) const
{ scene.UpdateSettingsCamera(); }

const BaseTitleState& GetSettingsState(TitleViewState type)
{
    static const SettingsTransitionState enterSettings(true), leaveSettings(false);
    static const SettingsState settings;
    if (type == TitleViewState::EnteringSettings) { return enterSettings; }
    if (type == TitleViewState::LeavingSettings) { return leaveSettings; }
    return settings;
}

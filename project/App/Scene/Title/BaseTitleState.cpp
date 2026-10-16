#include "BaseTitleState.h"
#include "TitleScene.h"
#include "Engine/Input/Input.h"
#include "Engine/audio/SoundManager.h"
#include <array>

void BaseTitleState::Enter(TitleScene& scene) const { scene.animationTime_ = 0.0f; }

class TitleMenuState final : public BaseTitleState {
public:
    TitleViewState GetType() const override { return TitleViewState::Title; }
    void HandleInput(TitleScene& scene, const Input& input) const override {
        if (input.IsKeyTrigger(DIK_SPACE) || input.IsKeyTrigger(DIK_RETURN) ||
            input.IsGamepadButtonTrigger(XINPUT_GAMEPAD_A)) {
            scene.StartGame();
        } else if (input.IsKeyTrigger(DIK_S) || input.IsGamepadButtonTrigger(XINPUT_GAMEPAD_Y)) {
            scene.StartSettingsTransition(true);
        }
    }
    void Update(TitleScene& scene, float dt) const override { scene.UpdateTitleCamera(dt); }
};

class TutorialStartState final : public BaseTitleState {
public:
    TitleViewState GetType() const override { return TitleViewState::StartingTutorial; }
    void Enter(TitleScene& scene) const override {
        BaseTitleState::Enter(scene);
        scene.InitializeTutorialAnimation();
    }
    void Update(TitleScene& scene, float dt) const override { scene.UpdateTutorialStart(dt); }
};

class StageSelectState final : public BaseTitleState {
public:
    TitleViewState GetType() const override { return TitleViewState::StageSelect; }
    void HandleInput(TitleScene& scene, const Input& input) const override {
        if (input.IsKeyTrigger(DIK_BACK) || input.IsGamepadButtonTrigger(XINPUT_GAMEPAD_B)) {
            scene.StartTitleReturn();
        } else if (!scene.stages_.empty() && !scene.stageRoomTransitionActive_ &&
            (input.IsKeyTrigger(DIK_D) || input.IsKeyTrigger(DIK_RIGHT) ||
                input.IsGamepadButtonTrigger(XINPUT_GAMEPAD_DPAD_RIGHT))) {
            scene.StartStageRoomTransition(1);
        } else if (!scene.stages_.empty() && !scene.stageRoomTransitionActive_ &&
            (input.IsKeyTrigger(DIK_A) || input.IsKeyTrigger(DIK_LEFT) ||
                input.IsGamepadButtonTrigger(XINPUT_GAMEPAD_DPAD_LEFT))) {
            scene.StartStageRoomTransition(-1);
        } else if (!scene.stageRoomTransitionActive_ &&
            (input.IsKeyTrigger(DIK_SPACE) || input.IsKeyTrigger(DIK_RETURN) ||
                input.IsGamepadButtonTrigger(XINPUT_GAMEPAD_A))) {
            scene.ConfirmStage();
        }
    }
    void Update(TitleScene& scene, float dt) const override { scene.UpdateStageSelection(dt); }
};

class StageSelectTransitionState final : public BaseTitleState {
public:
    explicit StageSelectTransitionState(bool entering) : entering_(entering) {}
    TitleViewState GetType() const override {
        return entering_ ? TitleViewState::EnteringStageSelect : TitleViewState::LeavingStageSelect;
    }
    void Enter(TitleScene& scene) const override {
        BaseTitleState::Enter(scene);
        if (!entering_) SoundManager::GetInstance()->Play("UiSelect");
    }
    void Update(TitleScene& scene, float dt) const override { scene.UpdateStageSelectTransition(dt); }
private:
    bool entering_;
};

class StageConfirmState final : public BaseTitleState {
public:
    TitleViewState GetType() const override { return TitleViewState::ConfirmingStage; }
    void Enter(TitleScene& scene) const override {
        BaseTitleState::Enter(scene);
        SoundManager::GetInstance()->Play("UiConfirm");
    }
    void Update(TitleScene& scene, float dt) const override { scene.UpdateStageConfirmation(dt); }
};

class SettingsState final : public BaseTitleState {
public:
    TitleViewState GetType() const override { return TitleViewState::Settings; }
    void HandleInput(TitleScene& scene, const Input& input) const override {
        if (input.IsKeyTrigger(DIK_BACK) || input.IsKeyTrigger(DIK_S) ||
            input.IsGamepadButtonTrigger(XINPUT_GAMEPAD_B)) {
            scene.StartSettingsTransition(false);
            return;
        }
        if (input.IsGamepadButtonTrigger(XINPUT_GAMEPAD_A) ||
            input.IsGamepadButtonTrigger(XINPUT_GAMEPAD_DPAD_LEFT) ||
            input.IsGamepadButtonTrigger(XINPUT_GAMEPAD_DPAD_RIGHT)) {
            Input::GetInstance()->ToggleGamepadPitchInverted();
        }
    }
    void Update(TitleScene& scene, float) const override { scene.UpdateSettingsCamera(); }
};

class SettingsTransitionState final : public BaseTitleState {
public:
    explicit SettingsTransitionState(bool opening) : opening_(opening) {}
    TitleViewState GetType() const override {
        return opening_ ? TitleViewState::EnteringSettings : TitleViewState::LeavingSettings;
    }
    void Update(TitleScene& scene, float dt) const override { scene.UpdateSettingsTransition(dt); }
private:
    bool opening_;
};

const BaseTitleState& GetTitleState(TitleViewState type)
{
    static const TitleMenuState title;
    static const TutorialStartState tutorial;
    static const StageSelectTransitionState enterStage(true), leaveStage(false);
    static const StageSelectState stage;
    static const StageConfirmState confirm;
    static const SettingsTransitionState enterSettings(true), leaveSettings(false);
    static const SettingsState settings;
    static const std::array<const BaseTitleState*, 9> states {
        &title, &tutorial, &enterStage, &stage, &leaveStage, &confirm, &enterSettings, &settings, &leaveSettings };
    const auto index = static_cast<size_t>(type);
    return *states[index < states.size() ? index : 0];
}

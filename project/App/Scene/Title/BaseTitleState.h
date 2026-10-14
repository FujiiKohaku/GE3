#pragma once

class TitleScene;
class Input;

enum class TitleViewState {
    Title, StartingTutorial, EnteringStageSelect, StageSelect, LeavingStageSelect,
    ConfirmingStage, EnteringSettings, Settings, LeavingSettings
};

class BaseTitleState {
public:
    virtual ~BaseTitleState() = default;
    virtual TitleViewState GetType() const = 0;
    virtual void Enter(TitleScene& scene) const;
    virtual void HandleInput(TitleScene&, const Input&) const {}
    virtual void Update(TitleScene& scene, float dt) const = 0;
    virtual void Exit(TitleScene&) const {}
};

const BaseTitleState& GetTitleState(TitleViewState type);

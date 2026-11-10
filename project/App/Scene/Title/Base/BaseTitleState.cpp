#include "BaseTitleState.h"
#include "App/Scene/Title/TitleScene/TitleScene.h"
#include "App/Scene/Title/TitleScene/Menu/TitleMenuStates.h"
#include "App/Scene/Title/TitleScene/StageSelect/StageSelectStates.h"
#include "App/Scene/Title/TitleScene/Settings/SettingsState.h"

void BaseTitleState::Enter(TitleScene& scene) const { scene.animationTime_ = 0.0f; }

const BaseTitleState& GetTitleState(TitleViewState type)
{
    switch (type) {
    case TitleViewState::Title:
    case TitleViewState::StartingTutorial:
        return GetTitleMenuState(type);
    case TitleViewState::EnteringStageSelect:
    case TitleViewState::StageSelect:
    case TitleViewState::LeavingStageSelect:
    case TitleViewState::ConfirmingStage:
        return GetStageSelectState(type);
    case TitleViewState::EnteringSettings:
    case TitleViewState::Settings:
    case TitleViewState::LeavingSettings:
        return GetSettingsState(type);
    }
    return GetTitleMenuState(TitleViewState::Title);
}

#include "Engine/Math/Easing.h"
#include "App/Scene/Title/TitleScene/Menu/TitleMenuStates.h"
#include "App/Scene/Title/TitleScene/TitleScene.h"
#include "Engine/Input/Input.h"
#include "Engine/audio/SoundManager.h"
#include "App/Scene/Common/SceneManager.h"
#include "App/Scene/Loading/LoadingScene.h"
#include "App/Scene/Gameplay/GamePlayScene.h"

// タイトル待機中の入力とカメラの更新を担当する。
TitleViewState TitleMenuState::GetType() const
{ return TitleViewState::Title; }

void TitleMenuState::HandleInput(TitleScene& scene, const Input& input) const
{
    if (input.IsKeyTrigger(DIK_SPACE) || input.IsKeyTrigger(DIK_RETURN) ||
        input.IsGamepadButtonTrigger(XINPUT_GAMEPAD_A)) {
        scene.StartGame();
    } else if (input.IsKeyTrigger(DIK_S) || input.IsGamepadButtonTrigger(XINPUT_GAMEPAD_Y)) {
        scene.StartSettingsTransition(true);
    }
}

void TitleMenuState::Update(TitleScene& scene, float dt) const
{ scene.UpdateTitleCamera(dt); }

// チュートリアルへ出発する演出の開始と進行を担当する。
TitleViewState TutorialStartState::GetType() const
{ return TitleViewState::StartingTutorial; }

void TutorialStartState::Enter(TitleScene& scene) const
{
    BaseTitleState::Enter(scene);
    scene.InitializeTutorialAnimation();
}

void TutorialStartState::Update(TitleScene& scene, float deltaTimeSeconds) const
{
    scene.animationTime_ += deltaTimeSeconds;
    const float progress = Easing::Clamp01(
        scene.animationTime_ / TitleTransitionSettings::kTutorialStartDurationSeconds);
    scene.UpdateTutorialStartPresentation(progress);
    if (progress >= 1.0f && !scene.stageTransitionQueued_) {
        scene.stageTransitionQueued_ = true;
        SceneManager::GetInstance()->SetNextScene(std::make_unique<LoadingScene>(
            std::make_unique<GamePlayScene>("tutorial"), true));
    }
}

const BaseTitleState& GetTitleMenuState(TitleViewState type)
{
    static const TitleMenuState title;
    static const TutorialStartState tutorial;
    if (type == TitleViewState::StartingTutorial) { return tutorial; }
    return title;
}

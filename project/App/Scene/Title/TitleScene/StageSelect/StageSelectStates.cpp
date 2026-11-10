#include "Engine/Math/Easing.h"
#include "App/Scene/Title/TitleScene/StageSelect/StageSelectStates.h"
#include "App/Scene/Title/TitleScene/TitleScene.h"
#include "Engine/Input/Input.h"
#include "Engine/audio/SoundManager.h"
#include "App/Scene/Common/SceneManager.h"
#include "App/Scene/Loading/LoadingScene.h"
#include "App/Scene/Gameplay/GamePlayScene.h"

namespace {
constexpr float kUpdateDistanceProgressOffset = 0.04f;
constexpr float kUpdateDistanceProgressDivisor = 0.96f;
}

// ステージ選択中の左右移動・決定・タイトルへ戻る入力を受け付ける。
TitleViewState StageSelectState::GetType() const
{ return TitleViewState::StageSelect; }

void StageSelectState::HandleInput(TitleScene& scene, const Input& input) const
{
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

void StageSelectState::Update(TitleScene& scene, float dt) const
{ scene.UpdateStageSelection(dt); }

// タイトルとステージ選択の間を行き来する遷移演出を進める。
StageSelectTransitionState::StageSelectTransitionState(bool isEntering) : isEntering_(isEntering)
{}

TitleViewState StageSelectTransitionState::GetType() const
{
    if (isEntering_) {
        return TitleViewState::EnteringStageSelect;
    }
    return TitleViewState::LeavingStageSelect;
}

void StageSelectTransitionState::Enter(TitleScene& scene) const
{
    BaseTitleState::Enter(scene);
    if (!isEntering_) SoundManager::GetInstance()->Play("UiSelect");
}

void StageSelectTransitionState::Update(TitleScene& scene, float deltaTimeSeconds) const
{
    scene.animationTime_ += deltaTimeSeconds;
    float durationSeconds = TitleTransitionSettings::kStageReturnDurationSeconds;
    if (isEntering_) {
        durationSeconds = TitleTransitionSettings::kStageEnterDurationSeconds;
    }
    const float progress = Easing::Clamp01(scene.animationTime_ / durationSeconds);
    float distanceProgress = 1.0f - Easing::SmoothStep(progress);
    if (isEntering_) {
        distanceProgress = Easing::SmoothStep((progress - kUpdateDistanceProgressOffset) / kUpdateDistanceProgressDivisor);
    }
    scene.UpdateStageSelectTransitionCamera(distanceProgress);
    if (progress >= 1.0f) {
        if (isEntering_) {
            scene.ChangeState(TitleViewState::StageSelect);
        } else {
            scene.ChangeState(TitleViewState::Title);
        }
    }
}

// 選択決定音を鳴らし、ステージへの出発演出を進める。
TitleViewState StageConfirmState::GetType() const
{ return TitleViewState::ConfirmingStage; }

void StageConfirmState::Enter(TitleScene& scene) const
{
    BaseTitleState::Enter(scene);
    SoundManager::GetInstance()->Play("UiConfirm");
}

void StageConfirmState::Update(TitleScene& scene, float deltaTimeSeconds) const
{
    scene.animationTime_ += deltaTimeSeconds;
    const float progress = Easing::Clamp01(
        scene.animationTime_ / TitleTransitionSettings::kStageConfirmDurationSeconds);
    scene.UpdateStageConfirmationCamera(progress);
    if (progress >= 1.0f && !scene.stageTransitionQueued_) {
        scene.stageTransitionQueued_ = true;
        SceneManager::GetInstance()->SetNextSceneWithLoading<LoadingScene, GamePlayScene>(
            scene.stages_[scene.currentStageIndex_].id);
    }
}

const BaseTitleState& GetStageSelectState(TitleViewState type)
{
    static const StageSelectTransitionState enterStage(true), leaveStage(false);
    static const StageSelectState stage;
    static const StageConfirmState confirm;
    if (type == TitleViewState::EnteringStageSelect) { return enterStage; }
    if (type == TitleViewState::LeavingStageSelect) { return leaveStage; }
    if (type == TitleViewState::ConfirmingStage) { return confirm; }
    return stage;
}

#include "Engine/Math/Easing.h"
#include "App/Scene/Title/TitleScene/Settings/SettingsTransitionState.h"
#include "App/Scene/Title/TitleScene/TitleScene.h"

// 設定画面を開く・閉じる方向に応じた遷移演出を進める。
SettingsTransitionState::SettingsTransitionState(bool isOpening) : isOpening_(isOpening)
{}

TitleViewState SettingsTransitionState::GetType() const
{
    if (isOpening_) {
        return TitleViewState::EnteringSettings;
    }
    return TitleViewState::LeavingSettings;
}

void SettingsTransitionState::Update(TitleScene& scene, float deltaTimeSeconds) const
{
    scene.animationTime_ += deltaTimeSeconds;
    const float progress = Easing::Clamp01(
        scene.animationTime_ / TitleTransitionSettings::kSettingsDurationSeconds);
    float settingsAmount = 1.0f - Easing::SmoothStep(progress);
    float whipDirection = 1.0f;
    if (isOpening_) {
        settingsAmount = Easing::EaseOutBack(progress);
        whipDirection = -1.0f;
    }
    scene.UpdateSettingsTransitionCamera(settingsAmount, progress, whipDirection);
    if (progress >= 1.0f) {
        if (isOpening_) {
            scene.ChangeState(TitleViewState::Settings);
        } else {
            scene.ChangeState(TitleViewState::Title);
            scene.camera_->SetFovY(0.64f);
        }
    }
}


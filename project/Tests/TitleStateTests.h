#pragma once
#include "App/Scene/Title/TitleScene.h"
#include <vector>

inline bool RunTitleStateTests()
{
    class RecordingState final : public BaseTitleState {
    public:
        RecordingState(TitleViewState type, int id, std::vector<int>& calls)
            : type_(type), id_(id), calls_(calls) {}
        TitleViewState GetType() const override { return type_; }
        void Enter(TitleScene&) const override { calls_.push_back(id_ * 10 + 1); }
        void Update(TitleScene&, float) const override { calls_.push_back(id_ * 10 + 2); }
        void Exit(TitleScene&) const override { calls_.push_back(id_ * 10 + 3); }
    private:
        TitleViewState type_;
        int id_;
        std::vector<int>& calls_;
    };
    for (int i = 0; i < 9; ++i) {
        const auto type = static_cast<TitleViewState>(i);
        if (GetTitleState(type).GetType() != type) return false;
    }
    TitleScene scene;
    std::vector<int> calls;
    RecordingState title(TitleViewState::Title, 1, calls);
    RecordingState settings(TitleViewState::Settings, 2, calls);
    scene.ChangeState(title);
    scene.currentState_->Update(scene, 0.1f);
    scene.ChangeState(settings);
    scene.currentState_->Update(scene, 0.1f);
    scene.ChangeState(settings); // 同じ状態への切り替えでは再初期化しない。
    if (calls != std::vector<int> {11, 12, 13, 21, 22} || scene.viewState_ != TitleViewState::Settings) return false;
    scene.animationTime_ = 4.0f;
    scene.ChangeState(TitleViewState::EnteringSettings);
    if (scene.animationTime_ != 0.0f || calls.back() != 23) return false;
    scene.animationTime_ = 3.0f;
    scene.ChangeState(TitleViewState::Title);
    if (scene.animationTime_ != 0.0f || scene.currentState_->GetType() != TitleViewState::Title) return false;

    // タイトルを作り直しても、今回の起動中に訓練を開始済みならセレクトへ進む。
    const bool hadStartedTutorial = TitleScene::hasStartedTutorial_;
    TitleScene::hasStartedTutorial_ = true;
    TitleScene returnedScene;
    returnedScene.StartGame();
    const bool hasOpenedStageSelect = returnedScene.viewState_ == TitleViewState::EnteringStageSelect;
    returnedScene.ChangeState(TitleViewState::Settings);
    returnedScene.StartGame();
    const bool hasKeptSettings = returnedScene.viewState_ == TitleViewState::Settings;
    TitleScene::hasStartedTutorial_ = hadStartedTutorial;
    return hasOpenedStageSelect && hasKeptSettings;
}

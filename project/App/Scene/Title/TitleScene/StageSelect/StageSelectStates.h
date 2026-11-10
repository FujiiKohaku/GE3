#pragma once
#include "App/Scene/Title/Base/BaseTitleState.h"

class StageSelectState final : public BaseTitleState {
public:
    TitleViewState GetType() const override;
    void HandleInput(TitleScene& scene, const Input& input) const override;
    void Update(TitleScene& scene, float dt) const override;
};

class StageSelectTransitionState final : public BaseTitleState {
public:
    explicit StageSelectTransitionState(bool isEntering);
    TitleViewState GetType() const override;
    void Enter(TitleScene& scene) const override;
    void Update(TitleScene& scene, float deltaTimeSeconds) const override;
private:
    bool isEntering_;
};

class StageConfirmState final : public BaseTitleState {
public:
    TitleViewState GetType() const override;
    void Enter(TitleScene& scene) const override;
    void Update(TitleScene& scene, float deltaTimeSeconds) const override;
};

const BaseTitleState& GetStageSelectState(TitleViewState type);

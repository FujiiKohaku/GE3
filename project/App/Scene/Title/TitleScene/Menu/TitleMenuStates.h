#pragma once
#include "App/Scene/Title/Base/BaseTitleState.h"

class TitleMenuState final : public BaseTitleState {
public:
    TitleViewState GetType() const override;
    void HandleInput(TitleScene& scene, const Input& input) const override;
    void Update(TitleScene& scene, float dt) const override;
};

class TutorialStartState final : public BaseTitleState {
public:
    TitleViewState GetType() const override;
    void Enter(TitleScene& scene) const override;
    void Update(TitleScene& scene, float deltaTimeSeconds) const override;
};

const BaseTitleState& GetTitleMenuState(TitleViewState type);

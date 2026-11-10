#pragma once
#include "App/Scene/Title/Base/BaseTitleState.h"

class SettingsTransitionState final : public BaseTitleState {
public:
    explicit SettingsTransitionState(bool isOpening);
    TitleViewState GetType() const override;
    void Update(TitleScene& scene, float deltaTimeSeconds) const override;
private:
    bool isOpening_;
};


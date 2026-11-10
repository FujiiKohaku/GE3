#pragma once
#include "App/Scene/Title/Base/BaseTitleState.h"

class SettingsState final : public BaseTitleState {
public:
    TitleViewState GetType() const override;
    void HandleInput(TitleScene& scene, const Input& input) const override;
    void Update(TitleScene& scene, float) const override;
};

const BaseTitleState& GetSettingsState(TitleViewState type);

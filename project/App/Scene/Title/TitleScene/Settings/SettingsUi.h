#pragma once
#include "Engine/2D/Sprite.h"
#include "Engine/2D/Text/Text.h"
#include <memory>

class Input;

class SettingsUi {
public:
    void Initialize(const char* fontPath, const char* texturePath);
    void Update(const Input* input, float settingsAmount, float titleOffset, float titleAlpha);
    void UpdateObjects();
    void DrawBackground();
    void DrawShortcut();
    void DrawSettingsText();

private:
    std::unique_ptr<Sprite> settingsBackdrop_;
    std::unique_ptr<Sprite> settingsAccent_;
    std::unique_ptr<Text> settingsShortcutText_;
    std::unique_ptr<Text> settingsTitleText_;
    std::unique_ptr<Text> settingsItemsText_;
    std::unique_ptr<Text> settingsBackText_;
};

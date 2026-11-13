#include "SettingsUi.h"
#include "Engine/2D/SpriteManager.h"
#include "Engine/Input/Input.h"
#include <algorithm>
#include <string>

namespace {
constexpr float kTextShadowAlpha = 0.6f;
constexpr Vector2 kInitializeInterfaceSettingsShortcutTextPosition = { 90.0f, 602.0f };
constexpr Vector4 kInitializeInterfaceSettingsShortcutTextColor = { 0.72f, 0.86f, 0.94f, 1.0f };
constexpr Vector4 kInitializeInterfaceSettingsBackdropColor = { 0.015f, 0.035f, 0.09f, 0.0f };
constexpr Vector4 kInitializeInterfaceSettingsAccentColor = { 0.10f, 0.86f, 1.0f, 0.0f };
constexpr Vector4 kInitializeInterfaceSettingsTitleTextColor = { 1.0f, 0.78f, 0.22f, 0.0f };
constexpr Vector4 kInitializeInterfaceSettingsItemsTextColor = { 0.90f, 0.96f, 1.0f, 0.0f };
constexpr Vector4 kInitializeInterfaceSettingsBackTextColor = { 0.62f, 0.84f, 0.94f, 0.0f };
constexpr float kSettingsTitleOutlineWidth = 2.0f;
constexpr float kSettingsShortcutBaseX = 90.0f;
constexpr float kSettingsShortcutExitOffsetScale = 1.34f;
constexpr float kSettingsShortcutY = 602.0f;
constexpr float kUpdateInterfaceSettingsShortcutTextColorRed = 0.72f;
constexpr float kUpdateInterfaceSettingsShortcutTextColorGreen = 0.86f;
constexpr float kUpdateInterfaceSettingsShortcutTextColorBlue = 0.94f;
constexpr float kUpdateInterfaceSettingsBackdropPositionY = 360.0f;
constexpr float kUpdateInterfaceSettingsAccentPositionX = 470.0f;
constexpr float kUpdateInterfaceSettingsAccentPositionY = 360.0f;
constexpr float kUpdateInterfaceSettingsBackdropColorRed = 0.015f;
constexpr float kUpdateInterfaceSettingsBackdropColorGreen = 0.035f;
constexpr float kUpdateInterfaceSettingsBackdropColorBlue = 0.09f;
constexpr float kUpdateInterfaceSettingsBackdropColorAlpha = 0.94f;
constexpr float kUpdateInterfaceSettingsAccentColorRed = 0.10f;
constexpr float kUpdateInterfaceSettingsAccentColorGreen = 0.86f;
constexpr float kUpdateInterfaceSettingsAccentColorAlpha = 0.95f;
constexpr float kUpdateInterfaceSettingsTitleTextPositionY = 104.0f;
constexpr float kUpdateInterfaceSettingsTitleTextColorGreen = 0.78f;
constexpr float kUpdateInterfaceSettingsTitleTextColorBlue = 0.22f;
constexpr float kUpdateInterfaceSettingsItemsTextPositionX = 48.0f;
constexpr float kUpdateInterfaceSettingsItemsTextPositionY = 248.0f;
constexpr float kUpdateInterfaceSettingsItemsTextColorRed = 0.90f;
constexpr float kUpdateInterfaceSettingsItemsTextColorGreen = 0.96f;
constexpr float kUpdateInterfaceSettingsBackTextPositionX = 48.0f;
constexpr float kUpdateInterfaceSettingsBackTextPositionY = 626.0f;
constexpr float kUpdateInterfaceSettingsBackTextColorRed = 0.62f;
constexpr float kUpdateInterfaceSettingsBackTextColorGreen = 0.84f;
constexpr float kUpdateInterfaceSettingsBackTextColorBlue = 0.94f;
}

void SettingsUi::Initialize(const char* fontPath, const char* texturePath)
{
    settingsShortcutText_ = std::make_unique<Text>();
    settingsShortcutText_->Initialize(fontPath);
    settingsShortcutText_->SetText("S  SETTINGS");
    settingsShortcutText_->SetPosition(kInitializeInterfaceSettingsShortcutTextPosition);
    settingsShortcutText_->SetFontSize(20.0f);
    settingsShortcutText_->SetColor(kInitializeInterfaceSettingsShortcutTextColor);
    settingsShortcutText_->SetOutlineWidth(1.0f);

    settingsBackdrop_ = std::make_unique<Sprite>();
    settingsBackdrop_->Initialize(SpriteManager::GetInstance(), texturePath);
    settingsBackdrop_->SetAnchorPoint({ 0.5f, 0.5f });
    settingsBackdrop_->SetSize({ 930.0f, 820.0f });
    settingsBackdrop_->SetRotation(-0.075f);
    settingsBackdrop_->SetColor(kInitializeInterfaceSettingsBackdropColor);

    settingsAccent_ = std::make_unique<Sprite>();
    settingsAccent_->Initialize(SpriteManager::GetInstance(), texturePath);
    settingsAccent_->SetAnchorPoint({ 0.5f, 0.5f });
    settingsAccent_->SetSize({ 84.0f, 900.0f });
    settingsAccent_->SetRotation(-0.18f);
    settingsAccent_->SetColor(kInitializeInterfaceSettingsAccentColor);

    settingsTitleText_ = std::make_unique<Text>();
    settingsTitleText_->Initialize(fontPath);
    settingsTitleText_->SetText("SETTINGS");
    settingsTitleText_->SetFontSize(68.0f);
    settingsTitleText_->SetRotation(-0.045f);
    settingsTitleText_->SetColor(kInitializeInterfaceSettingsTitleTextColor);
    settingsTitleText_->SetOutlineWidth(kSettingsTitleOutlineWidth);

    settingsItemsText_ = std::make_unique<Text>();
    settingsItemsText_->Initialize(fontPath);
    settingsItemsText_->SetText(
        "DISPLAY\n\nAUDIO\n\nCONTROLS\n\nACCESSIBILITY");
    settingsItemsText_->SetFontSize(29.0f);
    settingsItemsText_->SetLineSpacing(8.0f);
    settingsItemsText_->SetColor(kInitializeInterfaceSettingsItemsTextColor);

    settingsBackText_ = std::make_unique<Text>();
    settingsBackText_->Initialize(fontPath);
    settingsBackText_->SetText("BACKSPACE / S  BACK");
    settingsBackText_->SetFontSize(19.0f);
    settingsBackText_->SetColor(kInitializeInterfaceSettingsBackTextColor);

}

void SettingsUi::Update(const Input* input, float settingsAmount, float titleOffset, float titleAlpha)
{
    const bool isGamepad = input->IsUsingGamepad();
    if (isGamepad) {
        settingsShortcutText_->SetText("Y  SETTINGS");
        settingsBackText_->SetText("B  BACK");
    } else {
        settingsShortcutText_->SetText("S  SETTINGS");
        settingsBackText_->SetText("BACKSPACE / S  BACK");
    }
    std::string styleLabel = "EX-ZODIAC";
    std::string movementLabel = "LS MOVE / A,RT FIRE";
    std::string pitchLabel = "NORMAL";
    if (input->IsGamepadPitchInverted()) { pitchLabel = "INVERTED"; }
    std::string pitchShortcut = " [P]";
    if (isGamepad) {
        pitchShortcut = " [A / D-PAD]";
    }
    settingsItemsText_->SetText("PAD STYLE: " + styleLabel +
        "\n\nPAD PITCH: " + pitchLabel + pitchShortcut +
        "\n\n" + movementLabel + "\nX BOOST / Y BRAKE");
    settingsShortcutText_->SetPosition(
        { kSettingsShortcutBaseX + titleOffset * kSettingsShortcutExitOffsetScale, kSettingsShortcutY });
    settingsShortcutText_->SetColor(
        { kUpdateInterfaceSettingsShortcutTextColorRed, kUpdateInterfaceSettingsShortcutTextColorGreen, kUpdateInterfaceSettingsShortcutTextColorBlue, titleAlpha });
    settingsShortcutText_->SetOutlineColor({ 0.0f, 0.0f, 0.0f, titleAlpha });
    settingsShortcutText_->SetShadowColor({ 0.0f, 0.0f, 0.0f, kTextShadowAlpha * titleAlpha });

    const float panelX = 1480.0f + (790.0f - 1480.0f) * settingsAmount;
    settingsBackdrop_->SetPosition({ panelX, kUpdateInterfaceSettingsBackdropPositionY });
    settingsAccent_->SetPosition({ panelX - kUpdateInterfaceSettingsAccentPositionX, kUpdateInterfaceSettingsAccentPositionY });
    settingsBackdrop_->SetColor(
        { kUpdateInterfaceSettingsBackdropColorRed, kUpdateInterfaceSettingsBackdropColorGreen, kUpdateInterfaceSettingsBackdropColorBlue, kUpdateInterfaceSettingsBackdropColorAlpha * std::clamp(settingsAmount, 0.0f, 1.0f) });
    settingsAccent_->SetColor(
        { kUpdateInterfaceSettingsAccentColorRed, kUpdateInterfaceSettingsAccentColorGreen, 1.0f, kUpdateInterfaceSettingsAccentColorAlpha * std::clamp(settingsAmount, 0.0f, 1.0f) });

    const float textX = 1420.0f + (560.0f - 1420.0f) * settingsAmount;
    settingsTitleText_->SetPosition({ textX, kUpdateInterfaceSettingsTitleTextPositionY });
    settingsTitleText_->SetColor(
        { 1.0f, kUpdateInterfaceSettingsTitleTextColorGreen, kUpdateInterfaceSettingsTitleTextColorBlue, std::clamp(settingsAmount, 0.0f, 1.0f) });
    settingsItemsText_->SetPosition({ textX + kUpdateInterfaceSettingsItemsTextPositionX, kUpdateInterfaceSettingsItemsTextPositionY });
    settingsItemsText_->SetColor(
        { kUpdateInterfaceSettingsItemsTextColorRed, kUpdateInterfaceSettingsItemsTextColorGreen, 1.0f, std::clamp(settingsAmount, 0.0f, 1.0f) });
    settingsBackText_->SetPosition({ textX + kUpdateInterfaceSettingsBackTextPositionX, kUpdateInterfaceSettingsBackTextPositionY });
    settingsBackText_->SetColor(
        { kUpdateInterfaceSettingsBackTextColorRed, kUpdateInterfaceSettingsBackTextColorGreen, kUpdateInterfaceSettingsBackTextColorBlue, std::clamp(settingsAmount, 0.0f, 1.0f) });
    const float settingsAlpha = std::clamp(settingsAmount, 0.0f, 1.0f);
    settingsTitleText_->SetOutlineColor({ 0.0f, 0.0f, 0.0f, settingsAlpha });
    settingsTitleText_->SetShadowColor({ 0.0f, 0.0f, 0.0f, kTextShadowAlpha * settingsAlpha });
    settingsItemsText_->SetShadowColor({ 0.0f, 0.0f, 0.0f, kTextShadowAlpha * settingsAlpha });
    settingsBackText_->SetShadowColor({ 0.0f, 0.0f, 0.0f, kTextShadowAlpha * settingsAlpha });

}

void SettingsUi::UpdateObjects()
{
    settingsBackdrop_->Update();
    settingsAccent_->Update();
    settingsShortcutText_->Update();
    settingsTitleText_->Update();
    settingsItemsText_->Update();
    settingsBackText_->Update();
}

void SettingsUi::DrawBackground()
{
    settingsBackdrop_->Draw();
    settingsAccent_->Draw();
}

void SettingsUi::DrawShortcut()
{
    settingsShortcutText_->Draw();
}

void SettingsUi::DrawSettingsText()
{
    settingsTitleText_->Draw();
    settingsItemsText_->Draw();
    settingsBackText_->Draw();
}

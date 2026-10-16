#pragma once
#include "Tests/PlayerTestAccess.h"
#include "Engine/Winapp/WinApp.h"
#include <cmath>

struct InputTestAccess {
    static void Setup(Input& input, bool isUsingGamepad, bool isRightPressed, bool isUpPressed, SHORT stickX)
    {
        input.isUsingGamepad_ = isUsingGamepad;
        input.isGamepadConnected_ = isUsingGamepad;
        input.gamepadButtonsReady_ = true;
        input.gamepadState_.Gamepad = {};
        input.gamepadState_.Gamepad.sThumbLX = stickX;
        input.keys_[DIK_D] = 0;
        input.keys_[DIK_W] = 0;
        if (isRightPressed) { input.keys_[DIK_D] = 0x80; }
        if (isUpPressed) { input.keys_[DIK_W] = 0x80; }
        input.keys_[DIK_A] = 0;
        input.keys_[DIK_S] = 0;
        input.gamepadPitchInverted_ = false;
        input.gamepadAimSensitivity_ = 1.0f;
    }
};

inline bool RunPlayerControlTests()
{
    Input& input = *Input::GetInstance();
    Player player;
    PlayerTestAccess::InitializeAim(player);
    const Vector2 screen { static_cast<float>(WinApp::GetInstance()->GetClientWidth()),
        static_cast<float>(WinApp::GetInstance()->GetClientHeight()) };
    player.SetControlMode(Player::ControlMode::KeyboardAndMouse);
    InputTestAccess::Setup(input, false, true, true, 0);
    Vector2 steering = PlayerTestAccess::GetControlSteering(player, input, screen, false, 0.016f);
    if (steering.x != 1.0f || steering.y != 1.0f
        || PlayerTestAccess::GetControlRailResponse(player, input, screen, false, 0.016f) != 1.0f) { return false; }
    steering = PlayerTestAccess::GetControlSteering(player, input, screen, true, 0.016f);
    if (steering.x != 1.0f || steering.y != 1.0f) { return false; }
    InputTestAccess::Setup(input, false, false, false, 0);
    player.SetControlMode(Player::ControlMode::StarFox);
    PlayerTestAccess::UpdateGamepadAim(player, 1.0f, 0.0f, false, 0.1f);
    Player expected;
    PlayerTestAccess::UpdateSteering(expected, player.GetAimScreenPosition(), screen, false, 0.016f);
    steering = PlayerTestAccess::GetControlSteering(player, input, screen, false, 0.016f);
    if (std::abs(steering.x - expected.GetStarFoxSteeringInput().x) > 0.0001f
        || PlayerTestAccess::GetControlRailResponse(player, input, screen, false, 0.016f) != 2.75f) { return false; }
    PlayerTestAccess::InitializeAim(player);
    InputTestAccess::Setup(input, true, false, false, 32767);
    PlayerTestAccess::UpdateAimFromInput(player, input, 0.1f);
    if (player.GetAimScreenPosition().x <= screen.x * 0.5f) { return false; }
    PlayerTestAccess::UpdateSteering(expected, PlayerTestAccess::GetGamepadSteeringScreenPosition(player), screen, false, 0.016f, true);
    steering = PlayerTestAccess::GetControlSteering(player, input, screen, false, 0.016f);
    if (std::abs(steering.x - expected.GetStarFoxSteeringInput().x) > 0.0001f) { return false; }
    player.SetMouseSensitivity(2.0f);
    const Vector2 sensitivitySteering = PlayerTestAccess::GetControlSteering(player, input, screen, false, 0.016f);
    if (std::abs(sensitivitySteering.x - steering.x) > 0.0001f) { return false; }
    player.SetControlMode(Player::ControlMode::KeyboardAndMouse);
    if (!PlayerTestAccess::IsGamepadControl(player, input)) { return false; }
    InputTestAccess::Setup(input, false, false, false, 0);
    if (!PlayerTestAccess::IsKeyboardControl(player, input)) { return false; }
    player.SetControlMode(Player::ControlMode::StarFox);
    if (!PlayerTestAccess::IsMouseControl(player, input)) { return false; }
    const Vector2 before = player.GetAimScreenPosition();
    player.ConstrainGamepadAimToPlayer();
    InputTestAccess::Setup(input, true, false, false, 0);
    player.ConstrainGamepadAimToPlayer();
    return player.GetAimScreenPosition().x == before.x && player.GetAimScreenPosition().y == before.y;
}

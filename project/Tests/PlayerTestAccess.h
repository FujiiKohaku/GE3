#pragma once
#include "App/Game/Player/Player.h"
#include "Engine/Input/Input.h"

// 統合前の独立テストから、Player内の対応する処理を検証するための窓口。
struct PlayerTestAccess {
    static bool IsEntry(const Player& player) { return player.state_ == Player::State::Entry; }
    static bool IsBattle(const Player& player) { return player.state_ == Player::State::Battle; }
    static void UpdateEntry(Player& player) { player.entryState_.Update(player); }
    static EulerTransform& Transform(Player& player) { return player.transform_; }
    static void InitializeAim(Player& player) { player.InitializeAim(); }
    static void InitializeMovement(Player& player) { player.InitializeMovement(); }
    static void UpdateGamepadAim(Player& player, float stickX, float stickY, bool isPitchInverted,
        float deltaTimeSeconds, float sensitivity = 1.0f) { player.UpdateGamepadAim(stickX, stickY, isPitchInverted, deltaTimeSeconds, sensitivity); }
    static void ClampGamepadAimToPlayer(Player& player, const Vector2& position) { player.ClampGamepadAimToPlayer(position); }
    static Vector2 GetGamepadSteeringScreenPosition(const Player& player) { return player.GetGamepadSteeringScreenPosition(); }
    static void UpdateInvincibility(Player& player, float deltaTimeSeconds = 1.0f / 60.0f) { player.UpdateInvincibility(deltaTimeSeconds); }
    static void UpdateDeathAnimation(Player& player, EulerTransform& transform, float deltaTimeSeconds) { player.deadState_.UpdateAnimation(transform, deltaTimeSeconds); }
    static bool IsAlive(const Player& player) { return player.IsAlive(); }
    static bool IsFalling(const Player& player) { return player.IsFalling(); }
    static bool ShouldDraw(const Player& player) { return player.ShouldDraw(); }
    static void UpdateRoll(Player& player, EulerTransform& transform, bool isLeftTriggered,
        bool isRightTriggered, float deltaTimeSeconds = 1.0f / 60.0f) { player.UpdateRoll(transform, isLeftTriggered, isRightTriggered, deltaTimeSeconds); }
    static void StopRoll(Player& player) { player.StopRoll(); }
    static void UpdateSteering(Player& player, const Vector2& position, const Vector2& screenSize,
        bool isAllRangeMode, float deltaTimeSeconds, bool isGamepad = false) { player.UpdateSteering(position, screenSize, isAllRangeMode, deltaTimeSeconds, isGamepad); }
    static void ResetSteering(Player& player) { player.ResetSteering(); }
    static void UpdateMovement(Player& player, Input* input) { player.UpdateMovement(input); }
    static void StopMovement(Player& player) { player.StopMovement(); }
    static void UpdateBoost(Player& player, Input* input, bool isDebugMode) { player.UpdateBoost(input, isDebugMode); }
    static void ApplyRailPosition(Player& player) { player.ApplyRailPosition(); }
    static void SynchronizePosition(Player& player) { player.SynchronizePosition(); }
    static void SynchronizeOrientation(Player& player) { player.SynchronizeOrientation(); }
    static Vector3 ClampRailOffsetToScreen(const Player& player, const Vector3& offset) { return player.ClampRailOffsetToScreen(offset); }
    static Vector3 CalculateRailWorldPosition(const Player& player, const Vector3& offset) { return player.CalculateRailWorldPosition(offset); }
    static void UpdateAimFromInput(Player& player, const Input& input, float deltaTimeSeconds) { player.UpdateAimFromInput(input, deltaTimeSeconds); }
    static bool IsGamepadControl(const Player& player, const Input& input) { return player.GetControlInputType(input) == Player::ControlInputType::Gamepad; }
    static bool IsMouseControl(const Player& player, const Input& input) { return player.GetControlInputType(input) == Player::ControlInputType::Mouse; }
    static bool IsKeyboardControl(const Player& player, const Input& input) { return player.GetControlInputType(input) == Player::ControlInputType::Keyboard; }
    static Vector2 GetControlSteering(Player& player, const Input& input, const Vector2& screenSize,
        bool isAllRangeMode, float deltaTimeSeconds) { return player.GetControlInput(input, screenSize, isAllRangeMode, deltaTimeSeconds).steering; }
    static float GetControlRailResponse(Player& player, const Input& input, const Vector2& screenSize,
        bool isAllRangeMode, float deltaTimeSeconds) { return player.GetControlInput(input, screenSize, isAllRangeMode, deltaTimeSeconds).railResponse; }
};

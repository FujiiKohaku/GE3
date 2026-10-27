#pragma once
#include "Engine/math/MathStruct.h"

class Input;

// 入力機器の状態を、自機の操作に使う値へ変換する。PlayerとMovementは参照しない。
class PlayerInput {
public:
    enum class ControlInputType { Keyboard, Mouse, Gamepad };

    struct AimInput {
        Vector2 stick = {};
        bool isPitchInverted = false;
        float sensitivity = 1.0f;
    };

    struct MovementInput {
        float manualBank = 0.0f;
        bool isLeftRollTriggered = false;
        bool isRightRollTriggered = false;
    };

    struct BoostInput {
        bool isBraking = false;
        bool shouldBoost = false;
    };

    static ControlInputType GetControlInputType(const Input& input, bool shouldUseMouse);
    static Vector2 ReadMousePosition();
    static AimInput ReadGamepadAim(const Input& input);
    static Vector2 ReadKeyboardSteering(const Input& input);
    static MovementInput ReadMovement(const Input& input);
    static BoostInput ReadBoost(const Input& input, bool isEnabled);
};

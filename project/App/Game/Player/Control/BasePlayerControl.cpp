#include "BasePlayerControl.h"
#include "App/Game/Player/Aim/PlayerAimController.h"
#include "App/Game/Player/Movement/Steering/PlayerSteeringController.h"
#include "Engine/Input/Input.h"
#include "Engine/Camera/Camera.h"
#include <cmath>

namespace {
PlayerControlResult UpdateAimSteering(const PlayerControlContext& context, bool gamepad)
{
    const Vector2 position = gamepad ? context.aim.GetGamepadSteeringScreenPosition() : context.aim.GetAimScreenPosition();
    context.steering.Update(position, context.screenSize, context.allRange, context.dt, gamepad);
    return { context.steering.GetSteeringInput(), 2.75f };
}
}

class KeyboardPlayerControl final : public BasePlayerControl {
public:
    void UpdateAim(PlayerAimController& aim, const Input&, float) const override { aim.UpdateMouseAim(); }
    PlayerControlResult GetControl(const PlayerControlContext& context) const override {
        // 全方向飛行中は従来どおり照準操舵の内部状態も更新する。
        if (context.allRange) UpdateAimSteering(context, false);
        Vector2 input {};
        if (context.input.IsKeyPressed(DIK_A)) input.x -= 1.0f;
        if (context.input.IsKeyPressed(DIK_D)) input.x += 1.0f;
        if (context.input.IsKeyPressed(DIK_W)) input.y += 1.0f;
        if (context.input.IsKeyPressed(DIK_S)) input.y -= 1.0f;
        return { input, 1.0f };
    }
};

class MousePlayerControl final : public BasePlayerControl {
public:
    void UpdateAim(PlayerAimController& aim, const Input&, float) const override { aim.UpdateMouseAim(); }
    PlayerControlResult GetControl(const PlayerControlContext& context) const override { return UpdateAimSteering(context, false); }
};

class GamepadPlayerControl final : public BasePlayerControl {
public:
    void UpdateAim(PlayerAimController& aim, const Input& input, float dt) const override {
        aim.UpdateGamepadAim(input.GetGamepadLeftStickX(), input.GetGamepadLeftStickY(),
            input.IsGamepadPitchInverted(), dt, input.GetGamepadAimSensitivity());
    }
    PlayerControlResult GetControl(const PlayerControlContext& context) const override { return UpdateAimSteering(context, true); }
    void ConstrainAim(PlayerAimController& aim, const Camera* camera, const Vector3& position) const override {
        if (!camera) return;
        const Vector3 viewPosition = MatrixMath::Transform(position, camera->GetViewMatrix());
        if (!std::isfinite(viewPosition.z) || viewPosition.z <= camera->GetNearClip()) return;
        aim.ClampGamepadAimToPlayer(camera->WorldToScreen(position));
    }
};

const BasePlayerControl& GetPlayerControl(bool usingGamepad, bool mouseSteering)
{
    static const KeyboardPlayerControl keyboard;
    static const MousePlayerControl mouse;
    static const GamepadPlayerControl gamepad;
    if (usingGamepad) return gamepad;
    return mouseSteering ? static_cast<const BasePlayerControl&>(mouse) : static_cast<const BasePlayerControl&>(keyboard);
}

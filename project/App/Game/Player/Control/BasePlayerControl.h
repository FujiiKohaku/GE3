#pragma once
#include "Engine/math/MathStruct.h"

class Input;
class Camera;
class PlayerAimController;
class PlayerSteeringController;

struct PlayerControlContext {
    const Input& input;
    const PlayerAimController& aim;
    PlayerSteeringController& steering;
    Vector2 screenSize;
    bool allRange;
    float dt;
};

struct PlayerControlResult {
    Vector2 steering;
    float railResponse;
};

class BasePlayerControl {
public:
    virtual ~BasePlayerControl() = default;
    virtual void UpdateAim(PlayerAimController& aim, const Input& input, float dt) const = 0;
    virtual PlayerControlResult GetControl(const PlayerControlContext& context) const = 0;
    virtual void ConstrainAim(PlayerAimController&, const Camera*, const Vector3&) const {}
};

// パッドを使用中ならパッドを優先。その他は選択された操作モードを使う。
const BasePlayerControl& GetPlayerControl(bool usingGamepad, bool mouseSteering);

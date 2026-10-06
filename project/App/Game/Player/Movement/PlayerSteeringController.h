#pragma once
#include "Engine/math/MathStruct.h"

class PlayerSteeringController {
public:
    void Update(const Vector2& aimPosition, const Vector2& screenSizePixels,
        bool isAllRangeMode, float deltaTimeSeconds);
    void Reset() { steeringInput_ = {}; }
    void SetMouseSensitivity(float sensitivity);
    float GetMouseSensitivity() const { return mouseSensitivity_; }
    const Vector2& GetSteeringInput() const { return steeringInput_; }

private:
    float mouseSensitivity_ = 1.0f;
    Vector2 steeringInput_ = {};
};

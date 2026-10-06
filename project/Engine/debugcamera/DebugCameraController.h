#pragma once

#include "Engine/Camera/Camera.h"
class DebugCameraController {
public:
    void SetTargetCamera(Camera* camera);
    void Update();

    void SetDebugMode(bool isDebugMode);
    bool GetDebugMode() const { return isDebugMode_; }
    void SetArrowKeyRotationEnabled(bool enabled) { isArrowKeyRotationEnabled_ = enabled; }
    void SetRotationMouseButton(int button) { rotationMouseButton_ = button; }
    float GetMoveSpeed() const { return moveSpeed_; }
    void SetMoveSpeed(float speed);

private:
    Camera* targetCamera_ = nullptr;
    bool isDebugMode_ = false;
    bool isToggleKeyPressed_ = false;
    bool isArrowKeyRotationEnabled_ = true;
    int rotationMouseButton_ = 0;
    // World units per second, independent of frame rate.
    float moveSpeed_ = 30.0f;
    float normalFarClip_ = 1000.0f;
};

#pragma once

#include "Engine/Camera/Camera.h"
class DebugCameraController {
public:
    void SetTargetCamera(Camera* camera);
    void Update();

    void SetDebugMode(bool isDebugMode) { isDebugMode_ = isDebugMode; }
    bool GetDebugMode() const { return isDebugMode_; }
    void SetArrowKeyRotationEnabled(bool enabled) { isArrowKeyRotationEnabled_ = enabled; }
    void SetRotationMouseButton(int button) { rotationMouseButton_ = button; }

private:
    Camera* targetCamera_ = nullptr;
    bool isDebugMode_ = false;
    bool isToggleKeyPressed_ = false;
    bool isArrowKeyRotationEnabled_ = true;
    int rotationMouseButton_ = 0;
};

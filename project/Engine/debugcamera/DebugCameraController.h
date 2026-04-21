#pragma once

#include "Engine/Camera/Camera.h"
class DebugCameraController {
public:
    void SetTargetCamera(Camera* camera);
    void Update();

    void SetDebugMode(bool isDebugMode) { isDebugMode_ = isDebugMode; }
    bool GetDebugMode() const { return isDebugMode_; }

private:
    Camera* targetCamera_ = nullptr;
    bool isDebugMode_ = false;
    bool isToggleKeyPressed_ = false;
};
#pragma once
#include "Camera.h"

class DebugCameraController {
public:
    void SetTargetCamera(Camera* camera);

    void Update();

private:
    Camera* targetCamera_ = nullptr;
};
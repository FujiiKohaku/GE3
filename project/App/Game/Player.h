#pragma once
#include "../../Engine/3D/Model.h"
#include "../../Engine/3D/Object3d.h"
#include"../../Engine/math/MathStruct.h"
#include <memory>
#include"../../Engine/debugcamera/DebugCameraController.h"
class Camera;

class Player {
public:
    void Initialize(Model* model);
    void Update();
    void Draw();
    void SetCamera(Camera* camera);
    void SetDebugCameraController(DebugCameraController* debugCameraController);

private:
    std::unique_ptr<Object3d> object_;
    Camera* camera_ = nullptr;
    DebugCameraController* debugCameraController_ = nullptr;
    EulerTransform transform_;
    float moveSpeed_ = 0.2f;
    bool isDebugMode = false;
};
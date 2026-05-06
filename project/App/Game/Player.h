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
    const Vector3& GetTranslate() const;

private:
    std::unique_ptr<Object3d> object_;
    Camera* camera_ = nullptr;
    DebugCameraController* debugCameraController_ = nullptr;
    EulerTransform transform_;
    float moveSpeed_ = 0.2f;
    bool isDebugMode = false;
    Vector3 velocity_ = { 0.0f, 0.0f, 0.0f };

    float acceleration_ = 0.003f;
    float deceleration_ = 0.85f;
    float maxSpeed_ = 0.18f;

    float moveLimitX_ = 8.0f;
    float moveLimitZ_ = 5.0f;

    float tiltPower_ = 0.25f;
};
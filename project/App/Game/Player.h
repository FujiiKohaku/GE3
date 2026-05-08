#pragma once
#include "../../Engine/3D/Model.h"
#include "../../Engine/3D/Object3d.h"
#include"../../Engine/math/MathStruct.h"
#include <memory>
#include"../../Engine/debugcamera/DebugCameraController.h"

#include "../../Engine/Winapp/WinApp.h"
class Camera;

class Player {
public:
    void Initialize(Model* model);
    void Update();
    void Draw();
    void SetCamera(Camera* camera);
    void SetDebugCameraController(DebugCameraController* debugCameraController);
    const Vector3& GetTranslate() const
    {
        return transform_.translate;
    }


    const Vector3& GetRotate() const
    {
        return transform_.rotate;
    }

    const Vector3& GetScale() const
    {
        return transform_.scale;
    }
    void SetTranslate(const Vector3& translate)
    {
        transform_.translate = translate;
    }

    void SetRotate(const Vector3& rotate)
    {
        transform_.rotate = rotate;
    }

    void SetScale(const Vector3& scale)
    {
        transform_.scale = scale;
    }

    void SetWinApp(WinApp* winApp)
    {
        winApp_ = winApp;
    }

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

    WinApp* winApp_ = nullptr;
};
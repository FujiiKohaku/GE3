#pragma once
#include "../../Engine/3D/Model.h"
#include "../../Engine/3D/Object3d.h"
#include "../../Engine/Winapp/WinApp.h"
#include "../../Engine/debugcamera/DebugCameraController.h"
#include "../../Engine/math/MathStruct.h"
#include <memory>
#include <vector>

#include "Bullet.h"

class Camera;
class Input;

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

    const Vector2& GetAimScreenPosition() const
    {
        return aimScreenPosition_;
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

    const std::vector<std::unique_ptr<Bullet>>& GetBullets() const
    {
        return bullets_;
    }

    void DrawImGui();


private:
    std::unique_ptr<Object3d> object_;
    std::vector<std::unique_ptr<Bullet>> bullets_;

    Model* bulletModel_ = nullptr;
    Camera* camera_ = nullptr;
    DebugCameraController* debugCameraController_ = nullptr;

    EulerTransform transform_;

    bool isDebugMode = false;

    Vector2 aimScreenPosition_ = { 0.0f, 0.0f };

    float moveSpeed_ = 0.2f;
    Vector3 velocity_ = {0.0, 0.0, 0.5};
    float moveLimitX_ = 22.0f;
    float moveLimitY_ = 9.0f;
    float playerClampMarginX_ = 6.0f;
    float playerClampMarginY_ = 1.0f;

    float bulletSpawnOffsetY_ = 0.3f;
    float bulletSpawnOffsetZ_ = 4.0f;
    float bulletAimPowerX_ = 0.8f;
    float bulletAimPowerY_ = 0.4f;
    float bulletSpeed_ = 2.5f;

    void FireBullet();
    void UpdateBullets();
    void RemoveDeadBullets();
    void ApplyTransform();

    void UpdateKeyboardMove(Input* input);
    void UpdateMouseAim();
    void ClampAimScreenPosition();
    void ClampPlayerScreenPosition();
};
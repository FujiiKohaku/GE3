#include "DebugCameraController.h"
#include "Engine/input/Input.h"
#include "Engine/Time/TimeManager.h"
#include <algorithm>
#include <cmath>

void DebugCameraController::SetTargetCamera(Camera* camera)
{
    targetCamera_ = camera;
}

void DebugCameraController::SetDebugMode(bool isDebugMode)
{
    if (isDebugMode_ == isDebugMode) return;
    if (targetCamera_ != nullptr) {
        if (isDebugMode) {
            normalFarClip_ = targetCamera_->GetFarClip();
            targetCamera_->SetFarClip((std::max)(normalFarClip_, 3000.0f));
        } else {
            targetCamera_->SetFarClip(normalFarClip_);
        }
    }
    isDebugMode_ = isDebugMode;
}

void DebugCameraController::SetMoveSpeed(float speed)
{
    if (std::isfinite(speed)) {
        moveSpeed_ = std::clamp(speed, 1.0f, 1200.0f);
    }
}

void DebugCameraController::Update()
{
    if (targetCamera_ == nullptr) {
        return;
    }

    if (Input::GetInstance()->IsKeyPressed(DIK_F1)) {
        if (!isToggleKeyPressed_) {
            SetDebugMode(!isDebugMode_);
            isToggleKeyPressed_ = true;
        }
    } else {
        isToggleKeyPressed_ = false;
    }

    if (!isDebugMode_) {
        return;
    }

    const bool isUsingImGuiMouse =
#ifdef USE_IMGUI
        ImGui::GetIO().WantCaptureMouse;
#else
        false;
#endif
    if (!isUsingImGuiMouse) {
        const LONG wheel = Input::GetInstance()->GetMouseWheel();
        if (wheel != 0) {
            moveSpeed_ = std::clamp(
                moveSpeed_ * std::pow(1.25f, static_cast<float>(wheel) / 120.0f),
                1.0f, 1200.0f);
        }
    }
    const float deltaTime = std::clamp(
        TimeManager::GetInstance()->GetUnscaledDeltaTime(), 0.0f, 0.1f);
    const float moveSpeed = moveSpeed_ * deltaTime *
        (Input::GetInstance()->IsKeyPressed(DIK_LSHIFT) ||
         Input::GetInstance()->IsKeyPressed(DIK_RSHIFT) ? 5.0f : 1.0f);
    const float rotateSpeed = 0.05f;
    const float mouseSensitivity = 0.001f;

    Vector3 move;
    move.x = 0.0f;
    move.y = 0.0f;
    move.z = 0.0f;

    Vector3 cameraTranslate = targetCamera_->GetTranslate();
    Vector3 cameraRotate = targetCamera_->GetRotate();

    if (Input::GetInstance()->IsKeyPressed(DIK_W)) {
        move.z += moveSpeed;
    }

    if (Input::GetInstance()->IsKeyPressed(DIK_S)) {
        move.z -= moveSpeed;
    }

    if (Input::GetInstance()->IsKeyPressed(DIK_A)) {
        move.x -= moveSpeed;
    }

    if (Input::GetInstance()->IsKeyPressed(DIK_D)) {
        move.x += moveSpeed;
    }

    if (Input::GetInstance()->IsKeyPressed(DIK_Q)) {
        move.y -= moveSpeed;
    }

    if (Input::GetInstance()->IsKeyPressed(DIK_E)) {
        move.y += moveSpeed;
    }

    if (isArrowKeyRotationEnabled_) {
        if (Input::GetInstance()->IsKeyPressed(DIK_LEFT)) {
            cameraRotate.y += rotateSpeed;
        }

        if (Input::GetInstance()->IsKeyPressed(DIK_RIGHT)) {
            cameraRotate.y -= rotateSpeed;
        }

        if (Input::GetInstance()->IsKeyPressed(DIK_UP)) {
            cameraRotate.x += rotateSpeed;
        }

        if (Input::GetInstance()->IsKeyPressed(DIK_DOWN)) {
            cameraRotate.x -= rotateSpeed;
        }
    }

    if (isUsingImGuiMouse) {
        Input::GetInstance()->ResetMouseDelta();
        return;
    }

    if (Input::GetInstance()->IsMousePressed(rotationMouseButton_)) {
        cameraRotate.y += static_cast<float>(Input::GetInstance()->GetMouseDeltaX()) * mouseSensitivity;
        cameraRotate.x -= static_cast<float>(Input::GetInstance()->GetMouseDeltaY()) * mouseSensitivity;
    }

    if (cameraRotate.x > 1.5f) {
        cameraRotate.x = 1.5f;
    }

    if (cameraRotate.x < -1.5f) {
        cameraRotate.x = -1.5f;
    }

    cameraTranslate.x += move.x * std::cos(cameraRotate.y) - move.z * std::sin(cameraRotate.y);
    cameraTranslate.y += move.y;
    cameraTranslate.z += move.x * std::sin(cameraRotate.y) + move.z * std::cos(cameraRotate.y);

    targetCamera_->SetTranslate(cameraTranslate);
    targetCamera_->SetRotate(cameraRotate);
}

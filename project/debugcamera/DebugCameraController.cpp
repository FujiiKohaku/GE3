#include "DebugCameraController.h"
#include "Input.h"
#include <cmath>

void DebugCameraController::SetTargetCamera(Camera* camera)
{
    targetCamera_ = camera;
}

void DebugCameraController::Update()
{
    if (targetCamera_ == nullptr) {
        return;
    }

    if (Input::GetInstance()->IsKeyPressed(DIK_F1)) {
        if (!isToggleKeyPressed_) {
            isDebugMode_ = !isDebugMode_;
            isToggleKeyPressed_ = true;
        }
    } else {
        isToggleKeyPressed_ = false;
    }

    if (!isDebugMode_) {
        return;
    }

    const float moveSpeed = 0.1f;
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

    // マウスドラッグで回転
    if (!ImGui::GetIO().WantCaptureMouse && Input::GetInstance()->IsMousePressed(0)) {
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
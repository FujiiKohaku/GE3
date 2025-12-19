#include "Camera.h"
#include "DirectXCommon.h"
Camera::Camera()
    : transform_({ { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } })
    , fovY_(0.45f)
    , aspectRatio_(static_cast<float>(WinApp::kClientWidth) / static_cast<float>(WinApp::kClientHeight))
    , nearClip_(0.1f)
    , farClip_(100.0f)
    , worldMatrix_(MatrixMath::MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate))
    , viewMatrix_(MatrixMath::Inverse(worldMatrix_))
    , projectionMatrix_(MatrixMath::MakePerspectiveFovMatrix(fovY_, aspectRatio_, nearClip_, farClip_))
    , viewProjectionMatrix_(MatrixMath::Multiply(viewMatrix_, projectionMatrix_))
{
}

void Camera::Initialize()
{
    cameraResource_ = DirectXCommon::GetInstance()->CreateBufferResource(sizeof(CameraForGPU));

    cameraResource_->Map(0, nullptr, reinterpret_cast<void**>(&cameraData_));

}

void Camera::Update()
{
    assert(cameraData_ && "Camera::Initialize() is not called");
    cameraData_->worldPosition = transform_.translate;
    worldMatrix_ = MatrixMath::MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
    viewMatrix_ = MatrixMath::Inverse(worldMatrix_);
    projectionMatrix_ = MatrixMath::MakePerspectiveFovMatrix(0.45f, static_cast<float>(WinApp::kClientWidth) / static_cast<float>(WinApp::kClientHeight), 0.1f, 100.0f);
    viewProjectionMatrix_ = MatrixMath::Multiply(viewMatrix_, projectionMatrix_);
}

void Camera::DebugUpdate()
{
#ifdef USE_IMGUI

    ImGui::Begin("Settings");

    // 位置（移動）
    ImGui::DragFloat3("CameraTranslate", &transform_.translate.x, 0.01f, -10.0f, 10.0f);

    // 回転（角度）
    ImGui::SliderAngle("CameraRotateX", &transform_.rotate.x);
    ImGui::SliderAngle("CameraRotateY", &transform_.rotate.y);
    ImGui::SliderAngle("CameraRotateZ", &transform_.rotate.z);

    ImGui::End();

#endif
}

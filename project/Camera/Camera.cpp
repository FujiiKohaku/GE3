#include "Camera.h"
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

void Camera::Update()
{

    worldMatrix_ = MatrixMath::MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
    viewMatrix_ = MatrixMath::Inverse(worldMatrix_);
    projectionMatrix_ = MatrixMath::MakePerspectiveFovMatrix(0.45f, static_cast<float>(WinApp::kClientWidth) / static_cast<float>(WinApp::kClientHeight), 0.1f, 100.0f);
    viewProjectionMatrix_ = MatrixMath::Multiply(viewMatrix_, projectionMatrix_);
}
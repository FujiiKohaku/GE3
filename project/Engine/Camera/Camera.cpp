#include "Camera.h"
#include "Engine/DirectXCommon/DirectXCommon.h"
#include <cmath>
Camera::Camera()
    : transform_({ { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, -20.0f } })
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
    UpdateCameraResource();
    RecalculateMatrices();
}

void Camera::DebugUpdate()
{
    DrawImGui();
}

void Camera::LookAt(const Vector3& eye, const Vector3& target)
{
    transform_.translate = eye;

    Vector3 forward = Normalize(target - eye);
    if (forward.x == 0.0f && forward.y == 0.0f && forward.z == 0.0f) {
        return;
    }

    float horizontalLength = std::sqrt(forward.x * forward.x + forward.z * forward.z);

    transform_.rotate.x = -std::atan2(forward.y, horizontalLength);
    transform_.rotate.y = -std::atan2(forward.x, forward.z);
    transform_.rotate.z = 0.0f;
}

void Camera::DrawImGui()
{
#ifdef USE_IMGUI
    bool isTransformChanged = false;
    bool isProjectionChanged = false;

    ImGui::Begin("Camera");

    ImGui::Text("Transform");
    isTransformChanged |= ImGui::DragFloat3("Position", &transform_.translate.x, 0.01f, -100.0f, 100.0f);
    isTransformChanged |= ImGui::DragFloat3("Rotation", &transform_.rotate.x, 0.01f, -6.28f, 6.28f);
    isTransformChanged |= ImGui::DragFloat3("Scale", &transform_.scale.x, 0.01f, 0.01f, 10.0f);

    ImGui::Separator();
    ImGui::Text("Projection");
    isProjectionChanged |= ImGui::DragFloat("FovY", &fovY_, 0.01f, 0.01f, 3.13f);
    isProjectionChanged |= ImGui::DragFloat("NearClip", &nearClip_, 0.01f, 0.001f, farClip_ - 0.001f);
    isProjectionChanged |= ImGui::DragFloat("FarClip", &farClip_, 0.1f, nearClip_ + 0.001f, 10000.0f);

    if (fovY_ < 0.01f) {
        fovY_ = 0.01f;
        isProjectionChanged = true;
    }

    if (fovY_ > 3.13f) {
        fovY_ = 3.13f;
        isProjectionChanged = true;
    }

    if (nearClip_ < 0.001f) {
        nearClip_ = 0.001f;
        isProjectionChanged = true;
    }

    if (farClip_ <= nearClip_) {
        farClip_ = nearClip_ + 0.001f;
        isProjectionChanged = true;
    }

    if (isTransformChanged || isProjectionChanged) {
        UpdateCameraResource();
        RecalculateMatrices();
    }

    ImGui::End();

#endif
}

void Camera::RecalculateMatrices()
{
    worldMatrix_ = MatrixMath::MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
    viewMatrix_ = MatrixMath::Inverse(worldMatrix_);
    projectionMatrix_ = MatrixMath::MakePerspectiveFovMatrix(fovY_, aspectRatio_, nearClip_, farClip_);
    viewProjectionMatrix_ = MatrixMath::Multiply(viewMatrix_, projectionMatrix_);
}

void Camera::UpdateCameraResource()
{
    if (cameraData_ == nullptr) {
        return;
    }

    cameraData_->worldPosition = transform_.translate;
}

Camera::~Camera()
{
    if (cameraResource_) {
        cameraResource_->Unmap(0, nullptr);
    }
}
Vector2 Camera::WorldToScreen(const Vector3& worldPosition) const
{
    Matrix4x4 viewProjectionMatrix = MatrixMath::Multiply(GetViewMatrix(),GetProjectionMatrix());

    Vector3 clipPosition = MatrixMath::Transform(worldPosition,viewProjectionMatrix);

    Vector2 screenPosition;

    screenPosition.x = (clipPosition.x + 1.0f) * 0.5f * static_cast<float>(WinApp::GetInstance()->kClientWidth);

    screenPosition.y = (1.0f - clipPosition.y) * 0.5f * static_cast<float>(WinApp::GetInstance()->kClientHeight);

    return screenPosition;
}

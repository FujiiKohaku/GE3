#include "App/Game/Player/Aim/PlayerAimController.h"
#include "Engine/Winapp/WinApp.h"
#include "Engine/Camera/Camera.h"
#include "Engine/CollisionManager/CollisionManager.h"
#include <cmath>

void PlayerAimController::Initialize()
{
    aimScreenPosition_.x = static_cast<float>(WinApp::GetInstance()->GetClientWidth()) / 2.0f;
    aimScreenPosition_.y = static_cast<float>(WinApp::GetInstance()->GetClientHeight()) / 2.0f;
    smoothedAimDistance_ = kAimConvergenceDistance;
}
void PlayerAimController::ClampAimScreenPosition()
{
    constexpr float kHalfAimSizePixels = 64.0f;

    if (aimScreenPosition_.x < kHalfAimSizePixels) {
        aimScreenPosition_.x = kHalfAimSizePixels;
    }

    if (aimScreenPosition_.x > static_cast<float>(WinApp::GetInstance()->GetClientWidth()) - kHalfAimSizePixels) {
        aimScreenPosition_.x = static_cast<float>(WinApp::GetInstance()->GetClientWidth()) - kHalfAimSizePixels;
    }

    if (aimScreenPosition_.y < kHalfAimSizePixels) {
        aimScreenPosition_.y = kHalfAimSizePixels;
    }

    if (aimScreenPosition_.y > static_cast<float>(WinApp::GetInstance()->GetClientHeight()) - kHalfAimSizePixels) {
        aimScreenPosition_.y = static_cast<float>(WinApp::GetInstance()->GetClientHeight()) - kHalfAimSizePixels;
    }
}

void PlayerAimController::UpdateMouseAim()
{
    HWND hwnd = WinApp::GetInstance()->GetHwnd();

    POINT mousePosition;
    GetCursorPos(&mousePosition);
    ScreenToClient(hwnd, &mousePosition);

    aimScreenPosition_.x = static_cast<float>(mousePosition.x);
    aimScreenPosition_.y = static_cast<float>(mousePosition.y);
}

void PlayerAimController::CreateAimRay(Ray& aimRay, const Camera& activeCamera) const
{
    float mouseX = aimScreenPosition_.x;
    float mouseY = aimScreenPosition_.y;

    float screenWidth = static_cast<float>(WinApp::GetInstance()->GetClientWidth());
    float screenHeight = static_cast<float>(WinApp::GetInstance()->GetClientHeight());

    float ndcX = (2.0f * mouseX / screenWidth) - 1.0f;
    float ndcY = 1.0f - (2.0f * mouseY / screenHeight);

    Matrix4x4 inverseProjection = MatrixMath::Inverse(activeCamera.GetProjectionMatrix());
    Matrix4x4 inverseView = MatrixMath::Inverse(activeCamera.GetViewMatrix());

    Vector3 nearPoint = { ndcX, ndcY, 0.0f };
    Vector3 farPoint = { ndcX, ndcY, 1.0f };

    nearPoint = MatrixMath::Transform(nearPoint, inverseProjection);
    farPoint = MatrixMath::Transform(farPoint, inverseProjection);

    nearPoint = MatrixMath::Transform(nearPoint, inverseView);
    farPoint = MatrixMath::Transform(farPoint, inverseView);

    aimRay.origin = nearPoint;
    aimRay.direction = Normalize(farPoint - nearPoint);
}

Vector3 PlayerAimController::CreateConvergencePoint(const Ray& aimRay) const
{
    return aimRay.origin + aimRay.direction * kAimConvergenceDistance;
}

void PlayerAimController::UpdateSmoothedAimDistance(const Camera& activeCamera, float deltaTimeSeconds)
{
    Ray aimRay {};
    CreateAimRay(aimRay, activeCamera);

    float targetDistance = kAimConvergenceDistance;
    RaycastHit hit {};
    if (CollisionManager::GetInstance()->Raycast(aimRay, hit)) {
        targetDistance = hit.distance;
    }

    const float followRate = 1.0f - std::exp(-kAimDistanceFollowSpeed * deltaTimeSeconds);
    smoothedAimDistance_ += (targetDistance - smoothedAimDistance_) * followRate;
}

Vector3 PlayerAimController::ResolveAimPoint(const Ray& aimRay) const
{
    return aimRay.origin + aimRay.direction * smoothedAimDistance_;
}


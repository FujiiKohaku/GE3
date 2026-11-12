#include "App/Game/Player/Aim/PlayerShotPointCalculator.h"
#include "Engine/Camera/Camera.h"
#include "Engine/CollisionManager/CollisionManager.h"
#include "Engine/Winapp/WinApp.h"
#include "Engine/math/MatrixMath.h"
#include <algorithm>
#include <cmath>

void PlayerShotPointCalculator::Initialize()
{
    smoothedAimDistance_ = kAimConvergenceDistance;
    aimCamera_ = nullptr;
}

Vector3 PlayerShotPointCalculator::CalculateForwardPoint(const Vector3& shipPosition, const Vector3& shipDirection) const
{
    return shipPosition + shipDirection * kAimConvergenceDistance;
}

// 画面上の照準を通って奥へ伸びるレイを作り、引数のaimRayに結果を書き込む。
void PlayerShotPointCalculator::CreateAimRay(Ray& aimRay, const Camera& activeCamera, const Vector2& aimScreenPosition) const
{


    // 現在の照準位置を取得する
    float mouseX = aimScreenPosition.x;
    float mouseY = aimScreenPosition.y;

    // 画面の幅と高さを取得する
    float screenWidth = static_cast<float>(WinApp::GetInstance()->GetClientWidth());
    float screenHeight = static_cast<float>(WinApp::GetInstance()->GetClientHeight());

    // ピクセル座標をNDCへ変換する。画面中央は(0, 0)、左上は(-1, 1)。
    // 画面座標は下向きが正なので、Yを反転する。
    float ndcX = (2.0f * mouseX / screenWidth) - 1.0f;
    float ndcY = 1.0f - (2.0f * mouseY / screenHeight);

    // 描画時の座標変換を逆にたどるため、投影行列とビュー行列の逆行列を用意する。
    Matrix4x4 inverseProjection = MatrixMath::Inverse(activeCamera.GetProjectionMatrix());
    Matrix4x4 inverseView = MatrixMath::Inverse(activeCamera.GetViewMatrix());

    // 同じ照準位置のニア面とファー面に点を置く。Zの0と1はNDCの奥行き。
    Vector3 nearPoint = { ndcX, ndcY, 0.0f };
    Vector3 farPoint = { ndcX, ndcY, 1.0f };

    // 逆投影で、NDCからカメラ基準の3D座標へ戻す。
    nearPoint = MatrixMath::Transform(nearPoint, inverseProjection);
    farPoint = MatrixMath::Transform(farPoint, inverseProjection);

    // 逆ビュー変換で、カメラ基準の座標からワールド座標へ戻す。
    nearPoint = MatrixMath::Transform(nearPoint, inverseView);
    farPoint = MatrixMath::Transform(farPoint, inverseView);

    // ニア面の点を始点にする。方向は手前から奥へ向け、長さを1に揃える。
    aimRay.origin = nearPoint;
    aimRay.direction = Normalize(farPoint - nearPoint);
}

// 照準方向で最も近くに当たる対象までの距離へ、現在の照準距離を近づける。
void PlayerShotPointCalculator::Update(const Camera& activeCamera, const Vector2& aimScreenPosition, float deltaTimeSeconds)
{
    Ray aimRay {};
    CreateAimRay(aimRay, activeCamera, aimScreenPosition);
    UpdateFromRay(aimRay, activeCamera, aimScreenPosition, deltaTimeSeconds, kAimConvergenceDistance);
}

// レティクル位置からRayを作り、指定された3D座標を非ヒット時の基準にする。
void PlayerShotPointCalculator::UpdateFromWorldPoint(const Camera& activeCamera, const Vector2& aimScreenPosition,
    const Vector3& forwardPoint, float deltaTimeSeconds)
{
    Ray aimRay {};
    CreateAimRay(aimRay, activeCamera, aimScreenPosition);
    const float fallbackDistance = (std::max)(1.0f, Dot(forwardPoint - aimRay.origin, aimRay.direction));
    UpdateFromRay(aimRay, activeCamera, aimScreenPosition, deltaTimeSeconds, fallbackDistance);
}

void PlayerShotPointCalculator::UpdateFromRay(const Ray& aimRay, const Camera& activeCamera, const Vector2& aimScreenPosition,
    float deltaTimeSeconds, float fallbackDistance)
{

    // 何も当たらなければ基準距離を使い、当たれば命中点までの距離を使う。
    float targetDistance = fallbackDistance;
    RaycastHit hit {};
    if (CollisionManager::GetInstance()->Raycast(aimRay, hit)) {
        targetDistance = hit.distance;
    }

    // 対象が切り替わっても弾の収束位置が急変しないよう、奥行きだけを補間する。
    // 経過秒数から追従率を求め、フレームレートによる追従速度の差を抑える。
    if (isAimDistanceSmoothingEnabled_) {
        const float followRate = 1.0f - std::exp(-kAimDistanceFollowSpeed * deltaTimeSeconds);
        smoothedAimDistance_ += (targetDistance - smoothedAimDistance_) * followRate;
    } else {
        smoothedAimDistance_ = targetDistance;
    }
    aimRayOrigin_ = aimRay.origin;
    aimPoint_ = aimRay.origin + aimRay.direction * smoothedAimDistance_;
    aimCamera_ = &activeCamera;
    cachedAimScreenPosition_ = aimScreenPosition;
    cachedScreenWidthPixels_ = static_cast<float>(WinApp::GetInstance()->GetClientWidth());
    cachedScreenHeightPixels_ = static_cast<float>(WinApp::GetInstance()->GetClientHeight());
    cachedViewMatrix_ = activeCamera.GetViewMatrix();
    cachedProjectionMatrix_ = activeCamera.GetProjectionMatrix();
}

bool PlayerShotPointCalculator::IsAimCurrent(const Camera& activeCamera, const Vector2& aimScreenPosition) const
{
    if (aimCamera_ != &activeCamera ||
        cachedAimScreenPosition_.x != aimScreenPosition.x ||
        cachedAimScreenPosition_.y != aimScreenPosition.y ||
        cachedScreenWidthPixels_ != static_cast<float>(WinApp::GetInstance()->GetClientWidth()) ||
        cachedScreenHeightPixels_ != static_cast<float>(WinApp::GetInstance()->GetClientHeight())) { return false; }
    const Matrix4x4& viewMatrix = activeCamera.GetViewMatrix();
    const Matrix4x4& projectionMatrix = activeCamera.GetProjectionMatrix();
    for (int rowIndex = 0; rowIndex < 4; ++rowIndex) {
        for (int columnIndex = 0; columnIndex < 4; ++columnIndex) {
            if (cachedViewMatrix_.m[rowIndex][columnIndex] != viewMatrix.m[rowIndex][columnIndex] ||
                cachedProjectionMatrix_.m[rowIndex][columnIndex] != projectionMatrix.m[rowIndex][columnIndex]) {
                return false;
            }
        }
    }
    return true;
}

// 別のカメラが指定された場合は、補間済みの距離を使って目標点を計算する。
Vector3 PlayerShotPointCalculator::GetAimPoint(const Camera& activeCamera, const Vector2& aimScreenPosition) const
{
    if (IsAimCurrent(activeCamera, aimScreenPosition)) { return aimPoint_; }
    Ray aimRay {};
    CreateAimRay(aimRay, activeCamera, aimScreenPosition);
    return aimRay.origin + aimRay.direction * smoothedAimDistance_;
}

Vector3 PlayerShotPointCalculator::GetAimRayOrigin(const Camera& activeCamera, const Vector2& aimScreenPosition) const
{
    if (IsAimCurrent(activeCamera, aimScreenPosition)) { return aimRayOrigin_; }
    Ray aimRay {};
    CreateAimRay(aimRay, activeCamera, aimScreenPosition);
    return aimRay.origin;
}


#include "App/Game/Player/Aim/PlayerAimController.h"
#include "App/Game/Player/Player.h"
#include "Engine/Camera/Camera.h"
#include "Engine/CollisionManager/CollisionManager.h"
#include "Engine/Winapp/WinApp.h"
#include "Engine/math/MatrixMath.h"
#include <algorithm>
#include <cmath>

namespace {
constexpr float kMinGamepadSensitivity = 0.5f;
constexpr float kMaxGamepadSensitivity = 2.0f;
constexpr float kGamepadResponseBase = 0.5f;
constexpr float kGamepadResponseScale = 0.5f;
constexpr Vector2 kHalfReticleSizeUi = {
    PlayerAimController::kReticleSizeUi.x * 0.5f,
    PlayerAimController::kReticleSizeUi.y * 0.5f
};
}

// PlayerAimController
// 照準を画面中央に置き、狙う距離を基準値に戻す。
void PlayerAimController::InitializeAim()
{
    aimScreenPosition_.x = static_cast<float>(WinApp::GetInstance()->GetClientWidth()) / 2.0f;
    aimScreenPosition_.y = static_cast<float>(WinApp::GetInstance()->GetClientHeight()) / 2.0f;
    smoothedAimDistance_ = kAimConvergenceDistance;
    aimCamera_ = nullptr;
    gamepadAimOffset_ = {};
    hasGamepadAimAnchor_ = false;
}
// 照準画像がはみ出さないよう、画面端から半分のサイズだけ内側に制限する。
void PlayerAimController::ClampAimScreenPosition()
{
    const Vector2 halfAimSizePixels = WinApp::GetInstance()->UiToClient(kHalfReticleSizeUi);

    if (aimScreenPosition_.x < halfAimSizePixels.x) {
        aimScreenPosition_.x = halfAimSizePixels.x;
    }

    if (aimScreenPosition_.x > static_cast<float>(WinApp::GetInstance()->GetClientWidth()) - halfAimSizePixels.x) {
        aimScreenPosition_.x = static_cast<float>(WinApp::GetInstance()->GetClientWidth()) - halfAimSizePixels.x;
    }

    if (aimScreenPosition_.y < halfAimSizePixels.y) {
        aimScreenPosition_.y = halfAimSizePixels.y;
    }

    if (aimScreenPosition_.y > static_cast<float>(WinApp::GetInstance()->GetClientHeight()) - halfAimSizePixels.y) {
        aimScreenPosition_.y = static_cast<float>(WinApp::GetInstance()->GetClientHeight()) - halfAimSizePixels.y;
    }
}

// 画面上の機体から照準が離れすぎないように制限し、入力の内部位置も揃える。
void PlayerAimController::ClampGamepadAimToPlayer(const Vector2& playerScreenPosition)
{
    hasGamepadAimAnchor_ = false;
    const float width = static_cast<float>(WinApp::GetInstance()->GetClientWidth());
    const float height = static_cast<float>(WinApp::GetInstance()->GetClientHeight());
    const Vector2 halfAimSizePixels = WinApp::GetInstance()->UiToClient(kHalfReticleSizeUi);
    if (width <= halfAimSizePixels.x * 2.0f || height <= halfAimSizePixels.y * 2.0f ||
        !std::isfinite(playerScreenPosition.x) || !std::isfinite(playerScreenPosition.y)) {
        return;
    }
    // 画面外への投影時にも、照準画像が収まる領域を基準にする。
    const Vector2 anchor = {
        std::clamp(playerScreenPosition.x, halfAimSizePixels.x, width - halfAimSizePixels.x),
        std::clamp(playerScreenPosition.y, halfAimSizePixels.y, height - halfAimSizePixels.y)
    };
    gamepadAimAnchor_ = anchor;
    hasGamepadAimAnchor_ = true;
    ClampAimScreenPosition();
    const float dx = aimScreenPosition_.x - anchor.x;
    const float dy = aimScreenPosition_.y - anchor.y;
    const float maxDistance = (std::min)(width, height) * kGamepadMaxAimDistanceRatio;
    const float distanceSquared = dx * dx + dy * dy;
    if (distanceSquared > maxDistance * maxDistance) {
        const float scale = maxDistance / std::sqrt(distanceSquared);
        aimScreenPosition_ = { anchor.x + dx * scale, anchor.y + dy * scale };
    }
    // 表示位置と入力の内部位置を揃え、上限で倒し続けても余分な入力を蓄積しない。
    gamepadAimOffset_ = {
        (aimScreenPosition_.x - width * 0.5f) / (width * 0.5f),
        (aimScreenPosition_.y - height * 0.5f) / (height * 0.5f)
    };
}

// 機体からの照準のずれを、操舵処理が使う画面座標へ変換する。
Vector2 PlayerAimController::GetGamepadSteeringScreenPosition() const
{
    if (!hasGamepadAimAnchor_) {
        return aimScreenPosition_;
    }
    const float width = static_cast<float>(WinApp::GetInstance()->GetClientWidth());
    const float height = static_cast<float>(WinApp::GetInstance()->GetClientHeight());
    const float maxDistance = (std::min)(width, height) * kGamepadMaxAimDistanceRatio;
    if (maxDistance <= 0.0f) {
        return aimScreenPosition_;
    }
    return {
        width * 0.5f + (aimScreenPosition_.x - gamepadAimAnchor_.x) / maxDistance * width * 0.5f,
        height * 0.5f + (aimScreenPosition_.y - gamepadAimAnchor_.y) / maxDistance * height * 0.5f
    };
}

// デスクトップ上のマウス位置を、ゲームの描画領域の左上が原点の座標に変換する。
void PlayerAimController::UpdateMouseAim()
{
    aimScreenPosition_ = PlayerInput::ReadMousePosition();
    gamepadAimOffset_ = {};
    hasGamepadAimAnchor_ = false;
}

// スティック入力を照準の移動量に変換し、入力を離したら少しずつ中央へ戻す。
void PlayerAimController::UpdateGamepadAim(float stickX, float stickY, bool isPitchInverted, float deltaTimeSeconds, float sensitivity)
{
    hasGamepadAimAnchor_ = false;
    const float halfWidth = static_cast<float>(WinApp::GetInstance()->GetClientWidth()) * 0.5f;
    const float halfHeight = static_cast<float>(WinApp::GetInstance()->GetClientHeight()) * 0.5f;
    // 中央付近はゆっくり、倒し込むほど速く動かす。
    const float magnitude = std::sqrt(stickX * stickX + stickY * stickY);
    const float response = kGamepadResponseBase + kGamepadResponseScale * (std::min)(magnitude, 1.0f);
    stickX *= response;
    stickY *= response;
    const float dt = (std::max)(deltaTimeSeconds, 0.0f);
    constexpr float kAimMoveSpeed = 1.8f;
    constexpr float kAimReturnSpeed = 4.0f;
    const float moveSpeed = kAimMoveSpeed * std::clamp(sensitivity, kMinGamepadSensitivity, kMaxGamepadSensitivity);
    float pitchDirection = -1.0f;
    if (isPitchInverted) { pitchDirection = 1.0f; }
    if (magnitude > 0.0f) {
        // 操作中は速度として加算する。端までの距離で減速させない。
        gamepadAimOffset_.x += stickX * moveSpeed * dt;
        gamepadAimOffset_.y += stickY * pitchDirection * moveSpeed * dt;
    } else {
        // Inputのデッドゾーン内（スティックを離したとき）だけ中央へ戻す。
        const float remaining = std::exp(-kAimReturnSpeed * dt);
        gamepadAimOffset_.x *= remaining;
        gamepadAimOffset_.y *= remaining;
    }
    const Vector2 halfAimSizePixels = WinApp::GetInstance()->UiToClient(kHalfReticleSizeUi);
    const float limitX = (std::max)(halfWidth - halfAimSizePixels.x, 0.0f) / (std::max)(halfWidth, 1.0f);
    const float limitY = (std::max)(halfHeight - halfAimSizePixels.y, 0.0f) / (std::max)(halfHeight, 1.0f);
    // 照準画像が収まる位置まで動かせるよう、円形ではなく画面の矩形で制限する。
    gamepadAimOffset_.x = std::clamp(gamepadAimOffset_.x, -limitX, limitX);
    gamepadAimOffset_.y = std::clamp(gamepadAimOffset_.y, -limitY, limitY);
    aimScreenPosition_.x = halfWidth + gamepadAimOffset_.x * halfWidth;
    aimScreenPosition_.y = halfHeight + gamepadAimOffset_.y * halfHeight;
}

// 画面上の照準を通って奥へ伸びるレイを作り、引数のaimRayに結果を書き込む。
void PlayerAimController::CreateAimRay(Ray& aimRay, const Camera& activeCamera) const
{


    // 現在の照準位置を取得する
    float mouseX = aimScreenPosition_.x;
    float mouseY = aimScreenPosition_.y;

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
void PlayerAimController::Update(const Camera& activeCamera, float deltaTimeSeconds)
{
    Ray aimRay {};
    CreateAimRay(aimRay, activeCamera);
    UpdateFromRay(aimRay, activeCamera, deltaTimeSeconds, kAimConvergenceDistance);
}

// 機首の先を画面へ投影し、表示と発射で同じ照準点を使う。
void PlayerAimController::UpdateFromShip(const Camera& activeCamera, const Vector3& shipPosition,
    const Vector3& shipDirection, float deltaTimeSeconds)
{
    const Vector3 forwardPoint = shipPosition + shipDirection * kAimConvergenceDistance;
    aimScreenPosition_ = activeCamera.WorldToScreen(forwardPoint);
    ClampAimScreenPosition();
    hasGamepadAimAnchor_ = false;
    Ray aimRay {};
    CreateAimRay(aimRay, activeCamera);
    const float fallbackDistance = (std::max)(1.0f, Dot(forwardPoint - aimRay.origin, aimRay.direction));
    UpdateFromRay(aimRay, activeCamera, deltaTimeSeconds, fallbackDistance);
}

void PlayerAimController::UpdateFromRay(const Ray& aimRay, const Camera& activeCamera,
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
    cachedAimScreenPosition_ = aimScreenPosition_;
    cachedScreenWidthPixels_ = static_cast<float>(WinApp::GetInstance()->GetClientWidth());
    cachedScreenHeightPixels_ = static_cast<float>(WinApp::GetInstance()->GetClientHeight());
    cachedViewMatrix_ = activeCamera.GetViewMatrix();
    cachedProjectionMatrix_ = activeCamera.GetProjectionMatrix();
}

bool PlayerAimController::IsAimCurrent(const Camera& activeCamera) const
{
    if (aimCamera_ != &activeCamera ||
        cachedAimScreenPosition_.x != aimScreenPosition_.x ||
        cachedAimScreenPosition_.y != aimScreenPosition_.y ||
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
Vector3 PlayerAimController::GetAimPoint(const Camera& activeCamera) const
{
    if (IsAimCurrent(activeCamera)) { return aimPoint_; }
    Ray aimRay {};
    CreateAimRay(aimRay, activeCamera);
    return aimRay.origin + aimRay.direction * smoothedAimDistance_;
}

Vector3 PlayerAimController::GetAimRayOrigin(const Camera& activeCamera) const
{
    if (IsAimCurrent(activeCamera)) { return aimRayOrigin_; }
    Ray aimRay {};
    CreateAimRay(aimRay, activeCamera);
    return aimRay.origin;
}


#include "App/Game/Player/Aim/PlayerAimController.h"
#include "Engine/Winapp/WinApp.h"
#include "Engine/Camera/Camera.h"
#include "Engine/CollisionManager/CollisionManager.h"
#include <cmath>
#include <algorithm>

// 照準を画面中央に置き、狙う距離を基準値に戻す。
void PlayerAimController::Initialize()
{

    aimScreenPosition_.x = static_cast<float>(WinApp::GetInstance()->GetClientWidth()) / 2.0f;
    aimScreenPosition_.y = static_cast<float>(WinApp::GetInstance()->GetClientHeight()) / 2.0f;
    smoothedAimDistance_ = kAimConvergenceDistance;
    gamepadAimOffset_ = {};
    hasGamepadAimAnchor_ = false;
}
// 照準画像がはみ出さないよう、画面端から半分のサイズだけ内側に制限する。
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

void PlayerAimController::ClampGamepadAimToPlayer(const Vector2& playerScreenPosition)
{
    hasGamepadAimAnchor_ = false;
    const float width = static_cast<float>(WinApp::GetInstance()->GetClientWidth());
    const float height = static_cast<float>(WinApp::GetInstance()->GetClientHeight());
    constexpr float kHalfAimSizePixels = 64.0f;
    if (width <= kHalfAimSizePixels * 2.0f || height <= kHalfAimSizePixels * 2.0f ||
        !std::isfinite(playerScreenPosition.x) || !std::isfinite(playerScreenPosition.y)) {
        return;
    }
    // 画面外への投影時にも、照準画像が収まる領域を基準にする。
    const Vector2 anchor = {
        std::clamp(playerScreenPosition.x, kHalfAimSizePixels, width - kHalfAimSizePixels),
        std::clamp(playerScreenPosition.y, kHalfAimSizePixels, height - kHalfAimSizePixels)
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

    HWND hwnd = WinApp::GetInstance()->GetHwnd();

    POINT mousePosition;
    GetCursorPos(&mousePosition);
    ScreenToClient(hwnd, &mousePosition);

    aimScreenPosition_.x = static_cast<float>(mousePosition.x);
    aimScreenPosition_.y = static_cast<float>(mousePosition.y);
    gamepadAimOffset_ = {};
    hasGamepadAimAnchor_ = false;
}

void PlayerAimController::UpdateGamepadAim(float stickX, float stickY, bool invertedPitch, float deltaTimeSeconds, float sensitivity)
{
    hasGamepadAimAnchor_ = false;
    const float halfWidth = static_cast<float>(WinApp::GetInstance()->GetClientWidth()) * 0.5f;
    const float halfHeight = static_cast<float>(WinApp::GetInstance()->GetClientHeight()) * 0.5f;
    // 中央付近はゆっくり、倒し込むほど速く動かす。
    const float magnitude = std::sqrt(stickX * stickX + stickY * stickY);
    const float response = 0.5f + 0.5f * (std::min)(magnitude, 1.0f);
    stickX *= response;
    stickY *= response;
    const float dt = (std::max)(deltaTimeSeconds, 0.0f);
    constexpr float kAimMoveSpeed = 1.8f;
    constexpr float kAimReturnSpeed = 0.8f;
    const float moveSpeed = kAimMoveSpeed * std::clamp(sensitivity, 0.5f, 2.0f);
    if (magnitude > 0.0f) {
        // 操作中は速度として加算する。端までの距離で減速させない。
        gamepadAimOffset_.x += stickX * moveSpeed * dt;
        gamepadAimOffset_.y += stickY * (invertedPitch ? 1.0f : -1.0f) * moveSpeed * dt;
    } else {
        // Inputのデッドゾーン内（スティックを離したとき）だけ中央へ戻す。
        const float remaining = std::exp(-kAimReturnSpeed * dt);
        gamepadAimOffset_.x *= remaining;
        gamepadAimOffset_.y *= remaining;
    }
    constexpr float kHalfAimSizePixels = 64.0f;
    const float limitX = (std::max)(halfWidth - kHalfAimSizePixels, 0.0f) / (std::max)(halfWidth, 1.0f);
    const float limitY = (std::max)(halfHeight - kHalfAimSizePixels, 0.0f) / (std::max)(halfHeight, 1.0f);
    // 照準画像が収まる位置まで動かせるよう、円形ではなく画面の矩形で制限する。
    gamepadAimOffset_.x = std::clamp(gamepadAimOffset_.x, -limitX, limitX);
    gamepadAimOffset_.y = std::clamp(gamepadAimOffset_.y, -limitY, limitY);
    aimScreenPosition_.x = halfWidth + gamepadAimOffset_.x * halfWidth;
    aimScreenPosition_.y = halfHeight + gamepadAimOffset_.y * halfHeight;
}

// 画面上の照準を通って奥へ伸びるレイを作り、引数のaimRayに結果を書き込む。
void PlayerAimController::CreateAimRay(Ray& aimRay, const Camera& activeCamera) const
{
    float mouseX = aimScreenPosition_.x;
    float mouseY = aimScreenPosition_.y;

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

// 対象までの距離に関係なく、レイ上の基準距離にある点を返す。
Vector3 PlayerAimController::CreateConvergencePoint(const Ray& aimRay) const
{
    return aimRay.origin + aimRay.direction * kAimConvergenceDistance;
}

// 照準方向で最も近くに当たる対象までの距離へ、現在の照準距離を近づける。
void PlayerAimController::UpdateSmoothedAimDistance(const Camera& activeCamera, float deltaTimeSeconds)
{
    Ray aimRay {};
    CreateAimRay(aimRay, activeCamera);

    // 何も当たらなければ基準距離を使い、当たれば命中点までの距離を使う。
    float targetDistance = kAimConvergenceDistance;
    RaycastHit hit {};
    if (CollisionManager::GetInstance()->Raycast(aimRay, hit)) {
        targetDistance = hit.distance;
    }

    // 対象が切り替わっても弾の収束位置が急変しないよう、奥行きだけを補間する。
    // 経過秒数から追従率を求め、フレームレートによる追従速度の差を抑える。
    const float followRate = 1.0f - std::exp(-kAimDistanceFollowSpeed * deltaTimeSeconds);
    smoothedAimDistance_ += (targetDistance - smoothedAimDistance_) * followRate;
}

// 補間済みの距離を使い、銃口から弾を向けるためのワールド座標を返す。
Vector3 PlayerAimController::ResolveAimPoint(const Ray& aimRay) const
{
    return aimRay.origin + aimRay.direction * smoothedAimDistance_;
}


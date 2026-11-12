#include "App/Game/Player/Aim/PlayerReticleController.h"
#include "App/Game/Player/Player.h"
#include "Engine/Camera/Camera.h"
#include "Engine/Winapp/WinApp.h"
#include <algorithm>
#include <cmath>

namespace {
constexpr float kMinGamepadSensitivity = 0.5f;
constexpr float kMaxGamepadSensitivity = 2.0f;
constexpr float kGamepadResponseBase = 0.5f;
constexpr float kGamepadResponseScale = 0.5f;
constexpr Vector2 kHalfReticleSizeUi = {
    PlayerReticleController::kReticleSizeUi.x * 0.5f,
    PlayerReticleController::kReticleSizeUi.y * 0.5f
};
}

// PlayerReticleController
// レティクルを画面中央に置き、入力の内部位置を初期化する。
void PlayerReticleController::InitializeAim()
{
    aimScreenPosition_.x = static_cast<float>(WinApp::GetInstance()->GetClientWidth()) / 2.0f;
    aimScreenPosition_.y = static_cast<float>(WinApp::GetInstance()->GetClientHeight()) / 2.0f;
    gamepadAimOffset_ = {};
    hasGamepadAimAnchor_ = false;
}
// 照準画像がはみ出さないよう、画面端から半分のサイズだけ内側に制限する。
void PlayerReticleController::ClampAimScreenPosition()
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
void PlayerReticleController::ClampGamepadAimToPlayer(const Vector2& playerScreenPosition)
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
Vector2 PlayerReticleController::GetGamepadSteeringScreenPosition() const
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
void PlayerReticleController::UpdateMouseAim()
{
    aimScreenPosition_ = PlayerInput::ReadMousePosition();
    gamepadAimOffset_ = {};
    hasGamepadAimAnchor_ = false;
}

// スティック入力を照準の移動量に変換し、入力を離したら少しずつ中央へ戻す。
void PlayerReticleController::UpdateGamepadAim(float stickX, float stickY, bool isPitchInverted, float deltaTimeSeconds, float sensitivity)
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

// 機首の先など、指定された3D座標をレティクルの画面位置へ反映する。
void PlayerReticleController::UpdateFromWorldPoint(const Camera& activeCamera, const Vector3& worldPoint)
{
    aimScreenPosition_ = activeCamera.WorldToScreen(worldPoint);
    ClampAimScreenPosition();
    hasGamepadAimAnchor_ = false;
}

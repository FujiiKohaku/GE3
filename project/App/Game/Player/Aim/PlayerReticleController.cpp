#include "App/Game/Player/Aim/PlayerReticleController.h"
#include "App/Game/Player/Player.h"
#include "Engine/Camera/Camera.h"
#include "Engine/Input/Input.h"
#include "Engine/Winapp/WinApp.h"
#include <algorithm>
#include <cmath>

//、同じスティック入力から操舵と照準方向を作る。
Vector2 PlayerReticleController::UpdateDirectGamepadControl(const Input& input, float deltaTimeSeconds)
{
    Vector2 steering { input.GetGamepadLeftStickX(), input.GetGamepadLeftStickY() };


    //上下操作を反転する設定がONならスティックのYを反転させる。
    if (input.IsGamepadPitchInverted()) 
    { steering.y = -steering.y; }

    //スティックをどれだけ倒しているかを計算
    const float magnitude = std::sqrt(steering.x * steering.x + steering.y * steering.y);
    //少し倒したときの入力を弱めて、細かく操作しやすくしている
    constexpr float kMinStickResponse = 0.5f;
    const float response = kMinStickResponse +
        (1.0f - kMinStickResponse) * (std::min)(magnitude, 1.0f);
    //倍率をかけて方向をそのままにしたまま傾きの数値を調整している。
    steering.x *= response;
    steering.y *= response;
    //スティックに対するAIMの反応の速さ、小さいとゆっくり、大きいと早い
    constexpr float kDirectAimFollowSpeed = 4.5f;

    //FPSに依存せず指定した速さで目標に近づくためにやっている計算
    const float followRate = 1.0f - std::exp(-kDirectAimFollowSpeed * (std::max)(0.0f, deltaTimeSeconds));
    const float sensitivity = input.GetGamepadAimSensitivity();

    //感度を-1.0から1.0の値でクランプする
    //感度が高い場合はsteeringの数値が低くてもすぐ反応する。例: steering 0.2* 感度2倍 =0.4 
    const Vector2 targetAimInput {
        std::clamp(steering.x * sensitivity, -1.0f, 1.0f),
        std::clamp(steering.y * sensitivity, -1.0f, 1.0f)
    };


    // 3D空間の狙い先を求めるため、照準入力を滑らかに更新する。
    directGamepadAimInput_.x += (targetAimInput.x - directGamepadAimInput_.x) * followRate;
    directGamepadAimInput_.y += (targetAimInput.y - directGamepadAimInput_.y) * followRate;
    return steering;
}



// 機体の進行方向とレールの左右・上下方向から、3D空間で狙う方向を求める。
Vector3 PlayerReticleController::GetDirectGamepadAimDirection(bool isAllRangeMode,const Vector3& forward, const Vector3& right, const Vector3& up) const
{
    //全飛行モードだったら機体の進行方向をそのまま狙う方向にする
    if (isAllRangeMode)
    { return forward; }
    const float yawRadians = directGamepadAimInput_.x * kDirectAimYawRadians;
    const float pitchRadians = directGamepadAimInput_.y * kDirectAimPitchRadians;
    return Normalize(forward + right * std::tan(yawRadians) + up * std::tan(pitchRadians));
}

// カメラ確定後、機体から狙う方向の先にある点を画面上の照準へ反映する。
void PlayerReticleController::UpdateDirectGamepadAim(const Camera& activeCamera,
    const Vector3& playerPosition, bool isAllRangeMode, const Vector3& forward,
    const Vector3& right, const Vector3& up, float convergenceDistance)
{
    const Vector3 forwardPoint = playerPosition +
        GetDirectGamepadAimDirection(isAllRangeMode, forward, right, up) * convergenceDistance;
    UpdateFromWorldPoint(activeCamera, forwardPoint);
}

// 銃口から遠方の照準へ向かう射線上に、近距離表示用の点を置く。
Vector3 PlayerReticleController::GetNearAimPoint(const Vector3& muzzlePosition, const Vector3& aimPoint) const
{
    constexpr float kNearAimDistance = 45.0f;
    constexpr float kMinAimDistance = 0.0001f;
    constexpr float kNearAimMaxDistanceRatio = 0.5f;
    const Vector3 aimOffset = aimPoint - muzzlePosition;
    const float aimDistance = Vector3Length(aimOffset);
    if (aimDistance <= kMinAimDistance) { return muzzlePosition; }
    const float nearDistance = (std::min)(kNearAimDistance, aimDistance * kNearAimMaxDistanceRatio);
    return muzzlePosition + aimOffset * (nearDistance / aimDistance);
}

// PlayerReticleController
// レティクルを画面中央に置き、入力の内部位置を初期化する。
void PlayerReticleController::InitializeAim()
{
    aimScreenPosition_.x = static_cast<float>(WinApp::GetInstance()->GetClientWidth()) / 2.0f;
    aimScreenPosition_.y = static_cast<float>(WinApp::GetInstance()->GetClientHeight()) / 2.0f;
}
// 照準画像がはみ出さないよう、画面端から半分のサイズだけ内側に制限する。
void PlayerReticleController::ClampAimScreenPosition()
{
    const Vector2 halfAimSizePixels = WinApp::GetInstance()->UiToClient(kHalfReticleSizeUi);

    //画面の左側から照準が出ないようにする
    if (aimScreenPosition_.x < halfAimSizePixels.x) {
        aimScreenPosition_.x = halfAimSizePixels.x;
    }
    //画面の右側から照準が出ないようにする
    if (aimScreenPosition_.x > static_cast<float>(WinApp::GetInstance()->GetClientWidth()) - halfAimSizePixels.x) {
        aimScreenPosition_.x = static_cast<float>(WinApp::GetInstance()->GetClientWidth()) - halfAimSizePixels.x;
    }
    //画面の上側から照準が出ないようにする
    if (aimScreenPosition_.y < halfAimSizePixels.y) {
        aimScreenPosition_.y = halfAimSizePixels.y;
    }
    //画面の下側から照準が出ないようにする
    if (aimScreenPosition_.y > static_cast<float>(WinApp::GetInstance()->GetClientHeight()) - halfAimSizePixels.y) {
        aimScreenPosition_.y = static_cast<float>(WinApp::GetInstance()->GetClientHeight()) - halfAimSizePixels.y;
    }
}

// デスクトップ上のマウス位置を、ゲームの描画領域の左上が原点の座標に変換する。
void PlayerReticleController::UpdateMouseAim()
{
    aimScreenPosition_ = PlayerInput::ReadMousePosition();
}

// 機首の先など、指定された3D座標をレティクルの画面位置へ反映する。
void PlayerReticleController::UpdateFromWorldPoint(const Camera& activeCamera, const Vector3& worldPoint)
{
    aimScreenPosition_ = activeCamera.WorldToScreen(worldPoint);
    ClampAimScreenPosition();
}

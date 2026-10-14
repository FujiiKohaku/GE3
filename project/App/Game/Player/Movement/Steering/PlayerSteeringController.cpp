#include "PlayerSteeringController.h"
#include <algorithm>
#include <cmath>

namespace {
// 使用箇所：本ファイルのUpdate。中央付近の小さな入力を無視し、意図しない操舵を防ぐ。
float ApplySteeringDeadZone(float value)
{
    constexpr float kDeadZone = 0.05f;
    const float magnitude = std::abs(value);
    if (magnitude <= kDeadZone) {
        return 0.0f;
    }
    // デッドゾーンの外側を0～1へ引き伸ばし、元の左右・上下の符号を戻す。
    return std::copysign((magnitude - kDeadZone) / (1.0f - kDeadZone), value);
}
}

void PlayerSteeringController::SetMouseSensitivity(float sensitivity)
{
    // 極端に小さい・大きい感度にならないよう、設定値を制限する。
    mouseSensitivity_ = std::clamp(sensitivity, 0.5f, 2.0f);
}

void PlayerSteeringController::Update(const Vector2& aimPosition, const Vector2& screenSizePixels,
    bool isAllRangeMode, float deltaTimeSeconds, bool isGamepad)
{
    const float screenWidth = screenSizePixels.x;
    const float screenHeight = screenSizePixels.y;
    if (screenWidth <= 0.0f || screenHeight <= 0.0f) {
        // 画面サイズが無効なら、ゼロ除算を避けて現在の操舵入力を維持する。
        return;
    }

    // 照準をアナログスティックとして扱い、画面中央からのずれを入力へ変換する。
    // 中央は0、右端・上端は+1、左端・下端は-1。Yは画面座標と向きが逆になる。
    float inputX = (aimPosition.x - screenWidth * 0.5f) /(screenWidth * 0.5f);
    float inputY = (screenHeight * 0.5f - aimPosition.y) /(screenHeight * 0.5f);

    // 感度を掛けて-1～1へ制限し、中央付近の微小なずれを取り除く。
    // 照準の移動速度と機体の追従を分離する。パッドは照準のずれをそのまま操舵に使う。
    const float sensitivity = isGamepad ? 1.0f : mouseSensitivity_;
    inputX = std::clamp(inputX * sensitivity, -1.0f, 1.0f);
    inputY = std::clamp(inputY * sensitivity, -1.0f, 1.0f);
    // パッドのデッドゾーンはInputで適用済み。低感度でも細かな操作を残す。
    if (!isGamepad) {
        inputX = ApplySteeringDeadZone(inputX);
        inputY = ApplySteeringDeadZone(inputY);
    }

    // パッドは入力へ即時追従し、照準の移動・中央復帰にさらに遅れを重ねない。
    if (isGamepad) {
        steeringInput_ = { inputX, inputY };
        return;
    }
    // マウスのレール移動は即時追従し、全方向飛行では経過秒数から追従率を求める。
    // 全方向飛行の追従率は、フレームレートによる旋回感の差を抑えるため時間ベースにする。
    float steeringLerpRate = 1.0f;
    if (isAllRangeMode) {steeringLerpRate = 1.0f - std::exp(-13.4f * deltaTimeSeconds);
    }
    // 全方向飛行のマウス操舵には補間を適用する。
    steeringInput_.x +=(inputX - steeringInput_.x) * steeringLerpRate;
    steeringInput_.y +=(inputY - steeringInput_.y) * steeringLerpRate;

}

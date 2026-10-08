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
    bool isAllRangeMode, float deltaTimeSeconds)
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
    inputX = ApplySteeringDeadZone(std::clamp(
        inputX * mouseSensitivity_, -1.0f, 1.0f));
    inputY = ApplySteeringDeadZone(std::clamp(
        inputY * mouseSensitivity_, -1.0f, 1.0f));

    // レール移動では毎回20%追従し、全方向飛行では経過秒数から追従率を求める。
    // 全方向飛行の追従率は、フレームレートによる旋回感の差を抑えるため時間ベースにする。
    float steeringLerpRate = 0.20f;
    if (isAllRangeMode) {steeringLerpRate = 1.0f - std::exp(-13.4f * deltaTimeSeconds);
    }
    // 入力へ徐々に近づけ、マウスを急に動かしても操舵が瞬間的に切り替わらないようにする。
    steeringInput_.x +=(inputX - steeringInput_.x) * steeringLerpRate;
    steeringInput_.y +=(inputY - steeringInput_.y) * steeringLerpRate;

}

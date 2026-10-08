#pragma once
#include "Engine/math/MathStruct.h"

class PlayerSteeringController {
public:
    // 使用クラス：PlayerMovementController（UpdateStarFoxSteering）。照準位置から操舵入力を計算する。
    void Update(const Vector2& aimPosition, const Vector2& screenSizePixels,
        bool isAllRangeMode, float deltaTimeSeconds);
    // 使用クラス：PlayerMovementController（操作モード・全方向飛行への切り替え）。前の操舵入力を消す。
    void Reset() { steeringInput_ = {}; }
    // 使用クラス：PlayerMovementController（SetMouseSensitivity）。感度を0.5～2.0の範囲で設定する。
    void SetMouseSensitivity(float sensitivity);
    // 使用クラス：PlayerMovementController（GetMouseSensitivity）。設定中の感度を返す。
    float GetMouseSensitivity() const { return mouseSensitivity_; }
    // 使用クラス：PlayerMovementController（レール移動・全方向飛行・外部への公開）。補間済みの操舵入力を返す。
    const Vector2& GetSteeringInput() const { return steeringInput_; }

private:
    float mouseSensitivity_ = 1.0f; // 中央からの照準のずれに掛ける倍率。
    Vector2 steeringInput_ = {}; // 補間済みの入力。Xは右向き、Yは上向きが正で、中央は0。
};

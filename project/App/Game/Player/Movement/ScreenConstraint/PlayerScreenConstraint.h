#pragma once
#include "Engine/math/MathStruct.h"

class Camera;

class PlayerScreenConstraint {
public:
    // 使用クラス：PlayerMovementController（メンバー初期化）。レールの基準位置と方向を参照で受け取る。
    PlayerScreenConstraint(const Vector3& basePosition, const Vector3& right,const Vector3& up, const Vector3& forward): basePosition_(basePosition), right_(right), up_(up), forward_(forward) {}
    // 使用クラス：PlayerMovementController（SetCamera）。画面上への投影に使うカメラを設定する。
    void SetCamera(Camera* camera) { camera_ = camera; }
    // 使用クラス：PlayerMovementController（レール移動・外力の反映）。移動量を画面内へ補正する。
    Vector3 ClampRailOffsetToScreen(const Vector3& railOffset) const;
    // 使用クラス：PlayerMovementController（ApplyRailPosition）、本クラス（画面内補正）。ワールド位置へ変換する。
    Vector3 CalculateRailWorldPosition(const Vector3& railOffset) const;

private:
    // 使用クラス：本クラス（ClampRailOffsetToScreen）。画面内に戻すためのピクセル単位の補正量を求める。
    Vector2 CalculateScreenCorrection(const Vector3& railOffset) const;
    // 使用クラス：本クラス（CalculateScreenCorrection）。投影した点を含むように画面上の範囲を広げる。
    void UpdateScreenBounds(const Vector3& worldPosition, float& minX, float& maxX,float& minY, float& maxY) const;
    // 移動側のレール座標系を参照し、基準位置や方向の更新をそのまま反映する。
    const Vector3& basePosition_;
    const Vector3& right_;
    const Vector3& up_;
    const Vector3& forward_;
    Camera* camera_ = nullptr;
    float railMoveLimitX_ = 20.0f;
    float railMoveLimitY_ = 12.0f;
    // 画面端に残す余白。単位はピクセル。
    float playerClampMarginX_ = 100.0f;
    float playerClampMarginY_ = 100.0f;
    // 補正判定に使う機体の半幅・半高さ。単位はワールド座標。
    float playerBoundsHalfWidth_ = 1.5f;
    float playerBoundsHalfHeight_ = 1.0f;
};

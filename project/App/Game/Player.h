#pragma once
#include "../../Engine/3D/Model.h"
#include "../../Engine/3D/Object3d.h"
#include "../../Engine/Winapp/WinApp.h"
#include "../../Engine/debugcamera/DebugCameraController.h"
#include "../../Engine/math/MathStruct.h"
#include <memory>

class Camera;

class Player {
public:
    void Initialize(Model* model);
    void Update();
    void Draw();

    void SetCamera(Camera* camera);
    void SetDebugCameraController(DebugCameraController* debugCameraController);

    const Vector3& GetTranslate() const
    {
        return transform_.translate;
    }

    const Vector3& GetRotate() const
    {
        return transform_.rotate;
    }

    const Vector3& GetScale() const
    {
        return transform_.scale;
    }

    const Vector2& GetAimScreenPosition() const
    {
        return aimScreenPosition_;
    }

    void SetTranslate(const Vector3& translate)
    {
        transform_.translate = translate;
    }

    void SetRotate(const Vector3& rotate)
    {
        transform_.rotate = rotate;
    }

    void SetScale(const Vector3& scale)
    {
        transform_.scale = scale;
    }

private:
    // Player.h

    std::unique_ptr<Object3d> object_; // プレイヤーの3Dオブジェクト
    Camera* camera_ = nullptr; // カメラへのポインタ
    DebugCameraController* debugCameraController_ = nullptr; // デバッグカメラコントローラーへのポインタ
    EulerTransform transform_; // プレイヤーの位置、回転、スケールを管理する構造体

    bool isDebugMode = false; // デバッグモードのフラグ

    Vector3 velocity_ = { 0.0f, 0.0f, 0.0f }; // プレイヤーの現在の速度

    float acceleration_ = 0.02f; // 加速度
    float deceleration_ = 0.85f; // 減速率（0.0f～1.0fの範囲で設定、1.0fは減速なし）
    float maxSpeed_ = 0.18f; // 最大速度

    float moveLimitX_ = 8.0f; // X方向の移動制限
    float moveLimitY_ = 5.0f; // Y方向の移動制限

    float tiltPower_ = 0.25f;

    Vector3 aimPosition_ = { 0.0f, 0.0f, 0.0f };
    Vector2 aimScreenPosition_ = { 0.0f, 0.0f };

    Vector2 previousMousePosition_ = { 0.0f, 0.0f };

    bool isFirstMouseUpdate_ = true;

    float aimFollowPower_ = 0.08f;

    WinApp* winApp_ = nullptr;


    void ClampAimScreenPosition(); // 照準の画面上の位置を制限する関数
    void UpdateKeyboardAim(Input* input); // キーボードで照準を動かす処理
};
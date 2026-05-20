#pragma once
#include "../../Engine/3D/Model.h"
#include "../../Engine/3D/Object3d.h"
#include "../../Engine/Winapp/WinApp.h"
#include "../../Engine/debugcamera/DebugCameraController.h"
#include "../../Engine/math/MathStruct.h"
#include <memory>
#include <vector>

#include "Bullet.h"

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
    const std::vector<std::unique_ptr<Bullet>>& GetBullets() const
    {
        return bullets_;
    }
    void DrawImGui();

private:
    // Player.h

    std::unique_ptr<Object3d> object_; // プレイヤーの3Dオブジェクト
    std::vector<std::unique_ptr<Bullet>> bullets_; // プレイヤーが発射した弾のリスト

    Model* bulletModel_ = nullptr;
    Camera* camera_ = nullptr; // カメラへのポインタ
    DebugCameraController* debugCameraController_ = nullptr; // デバッグカメラコントローラーへのポインタ
    EulerTransform transform_; // プレイヤーの位置、回転、スケールを管理する構造体

    bool isDebugMode = false; // デバッグモードのフラグ

    Vector3 velocity_ = { 0.0f, 0.0f, 0.0f }; // プレイヤーの現在の速度

    float acceleration_ = 0.02f; // 加速度
    float deceleration_ = 0.85f; // 減速率（0.0f～1.0fの範囲で設定、1.0fは減速なし）
    float maxSpeed_ = 0.18f; // 最大速度

    float moveLimitX_ = 22.0f;
    float moveLimitY_ = 9.0f;

    float tiltPower_ = 0.25f;

    Vector3 aimPosition_ = { 0.0f, 0.0f, 0.0f };
    Vector2 aimScreenPosition_ = { 0.0f, 0.0f };
    float aimFollowPower_ = 0.01f;
    float aimScreenFollowPower_ = 1.0f;
    float mouseAimFollowPower_ = 0.08f;
    void ResetParameters();

    float bulletSpawnOffsetY_ = 0.3f;
    float bulletSpawnOffsetZ_ = 4.0f;
    float bulletAimPowerX_ = 0.8f;
    float bulletAimPowerY_ = 0.4f;
    float bulletSpeed_ = 2.5f;

    float keyboardAimScreenSpeed_ = 8.0f;
    float rotateFollowPower_ = 0.003f;
    float tiltAimPowerX_ = 0.04f;
    float tiltAimPowerY_ = 0.04f;
    float tiltRollPower_ = 0.03f;
    float playerClampMarginX_ = 6.0f;
    float playerClampMarginY_ = 1.0f;
  //  Vector2 previousMousePosition_ = { 0.0f, 0.0f };

  //  bool isFirstMouseUpdate_ = true;
    // なめらか追従
  //  float followSpeed = 0.5f;

  //  float mouseFollowRate_ = 0.15f;
    WinApp* winApp_ = nullptr;
   // Vector3 difference_;
   // float aimTiltPower_ = 0.08f;
    void FireBullet();
    void UpdateBullets();
    void RemoveDeadBullets();
    void ApplyTransform();


    void ClampAimScreenPosition(); // 照準の画面上の位置を制限する関数
    void UpdateKeyboardAim(Input* input); // キーボードで照準を動かす処理
    void UpdateMouseAim(); // マウスで照準を動かす処理
    void ClampWorldAimPosition(); // 照準のワールド上の位置を制限する処理
    void FollowAimPosition(); // プレイヤーの位置を照準に追従させる処理
    void UpdateTilt(); // プレイヤーの傾きを更新する処理
    void ClampPlayerWorldPosition(); // プレイヤーのワールド上の位置を制限する処理
};
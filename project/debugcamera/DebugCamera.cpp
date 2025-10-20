#include "DebugCamera.h"

void DebugCamera::Update()
{
    const float moveSpeed = 0.1f;
    const float rotateSpeed = 0.05f;
    input_.Update();
    // 移動ベクトル（ローカル空間）
    Vector3 move = { 0.0f, 0.0f, 0.0f };

    if (input_.IsKeyPressed(DIK_W)) {
        move.z -= moveSpeed;
    }
    if (input_.IsKeyPressed(DIK_S)) {
        move.z += moveSpeed;
    }
    if (input_.IsKeyPressed(DIK_A)) {
        move.x -= moveSpeed;
    }
    if (input_.IsKeyPressed(DIK_D)) {
        move.x += moveSpeed;
    }
    if (input_.IsKeyPressed(DIK_Q)) {
        move.y += moveSpeed;
        ;
    }
    if (input_.IsKeyPressed(DIK_E)) {
        move.y -= moveSpeed;
    }

    // Y軸回転（左右キーで）
    if (input_.IsKeyPressed(DIK_LEFT)) {
        rotation_.y -= rotateSpeed;
    }
    if (input_.IsKeyPressed(DIK_RIGHT)) {
        rotation_.y += rotateSpeed;
    }

    // 回転を考慮してワールド空間で移動
    translation_.x += move.x * std::cos(rotation_.y) - move.z * std::sin(rotation_.y);
    translation_.y += move.y;
    translation_.z += move.x * std::sin(rotation_.y) + move.z * std::cos(rotation_.y);

    // ビュー行列の更新
    cameraMatrix = DebugCamera::MakeAffineMatrix({ 1.0f, 1.0f, 1.0f }, rotation_, translation_);
    viewMatrix = DebugCamera::Inverse(cameraMatrix);
}

void DebugCamera::Initialize(WinApp* winApp)
{
    winApp_ = winApp;

    input_.Initialize(winApp);
    // ワールド変換行列を作成（スケール1、回転・移動はメンバ変数から）
    cameraMatrix = DebugCamera::MakeAffineMatrix({ 1.0f, 1.0f, 1.0f }, rotation_, translation_);

    // ビュー行列はカメラ行列の逆行列
    viewMatrix = DebugCamera::Inverse(cameraMatrix);

    // 正射影行列の作成（左上・右下・近クリップ・遠クリップ）
    orthoGraphicMatrix = DebugCamera::MakeOrthographicMatrix(-160.0f, 160.0f, 200.0f, 300.0f, 0.0f, 1000.0f);
}
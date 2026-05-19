#include "Player.h"
#include "../../Engine/3D/Object3dManager.h"
#include "../../Engine/Input/Input.h"
#include "../../Engine/debugcamera/DebugCameraController.h"
#include <cassert>

void Player::Initialize(Model* model)
{
    assert(model != nullptr);

    object_ = std::make_unique<Object3d>();
    object_->Initialize(Object3dManager::GetInstance());
    object_->SetModel(model);

    if (camera_ != nullptr) {
        object_->SetCamera(camera_);
    }

    transform_.scale = { 1.0f, 1.0f, 1.0f };
    transform_.rotate = { 0.0f, 0.0f, 0.0f };
    transform_.translate = { 0.0f, 0.0f, 0.0f };
    aimScreenPosition_.x = WinApp::GetInstance()->kClientWidth / 2.0f;
    aimScreenPosition_.y = WinApp::GetInstance()->kClientHeight / 2.0f;
    object_->SetScale(transform_.scale);
    object_->SetRotate(transform_.rotate);
    object_->SetTranslate(transform_.translate);


    aimPosition_ = { 0.0f, 0.0f, 0.0f };
    velocity_ = { 0.0f, 0.0f, 0.0f };

    aimScreenPosition_.x = WinApp::GetInstance()->kClientWidth / 2.0f;
    aimScreenPosition_.y = WinApp::GetInstance()->kClientHeight / 2.0f;
}

void Player::Update()
{
    // オブジェクトが初期化されていない場合は更新処理を行わない
    if (object_ == nullptr) {
        return;
    }

    // inputのインスタンスを取得
    Input* input = Input::GetInstance();

    // inputが初期化されていない場合は更新処理を行わない
    if (input == nullptr) {
        return;
    }

    // デバッグカメラコントローラーが存在する場合は、デバッグモードの状態を取得
    if (debugCameraController_ != nullptr) {
        isDebugMode = debugCameraController_->GetDebugMode();
    }

    if (!isDebugMode) { // デバックモードではない時はプレイヤーの移動処理を行う

        // ==============================
        // キーボードで照準を動かす
        // ==============================

        Vector3 inputDirection = { 0.0f, 0.0f, 0.0f };

        bool isKeyboardAimInput = false;

        // キー入力に応じて移動方向を設定
        if (input->IsKeyPressed(DIK_A)) {
            inputDirection.x -= 1.0f;
            isKeyboardAimInput = true;
        }

        if (input->IsKeyPressed(DIK_D)) {
            inputDirection.x += 1.0f;
            isKeyboardAimInput = true;
        }

        if (input->IsKeyPressed(DIK_W)) {
            inputDirection.y += 1.0f;
            isKeyboardAimInput = true;
        }

        if (input->IsKeyPressed(DIK_S)) {
            inputDirection.y -= 1.0f;
            isKeyboardAimInput = true;
        }
        // キーボード入力がある時だけ照準を動かす
        if (isKeyboardAimInput) {
            aimPosition_.x += velocity_.x;
            aimPosition_.y += velocity_.y;

            // 照準スプライトも動かす
            float keyboardAimScreenSpeed = 8.0f;

            aimScreenPosition_.x += inputDirection.x * keyboardAimScreenSpeed;
            aimScreenPosition_.y -= inputDirection.y * keyboardAimScreenSpeed;
        }
        // 斜め移動の速度補正
        float length = std::sqrt(
            inputDirection.x * inputDirection.x + inputDirection.y * inputDirection.y);

        // 正規化
        if (length > 0.0f) {
            inputDirection.x /= length;
            inputDirection.y /= length;
        }

        // 加速度を考慮して速度を更新
        velocity_.x += inputDirection.x * acceleration_;
        velocity_.y += inputDirection.y * acceleration_;

        // 最大速度を超えないように制限
        if (velocity_.x > maxSpeed_) {
            velocity_.x = maxSpeed_;
        }

        if (velocity_.x < -maxSpeed_) {
            velocity_.x = -maxSpeed_;
        }

        if (velocity_.y > maxSpeed_) {
            velocity_.y = maxSpeed_;
        }

        if (velocity_.y < -maxSpeed_) {
            velocity_.y = -maxSpeed_;
        }

        // 減速処理
        velocity_.x *= deceleration_;
        velocity_.y *= deceleration_;

        // キーボード入力がある時だけ照準を動かす
        if (isKeyboardAimInput) {
            aimPosition_.x += velocity_.x;
            aimPosition_.y += velocity_.y;
        }

// ==============================
// マウス移動量で照準を動かす
// ==============================

HWND hwnd = WinApp::GetInstance()->GetHwnd();

RECT clientRect;
GetClientRect(hwnd, &clientRect);

POINT centerMousePosition;
centerMousePosition.x = (clientRect.right - clientRect.left) / 2;
centerMousePosition.y = (clientRect.bottom - clientRect.top) / 2;

// クライアント座標の中心をスクリーン座標に変換
POINT centerScreenPosition = centerMousePosition;
ClientToScreen(hwnd, &centerScreenPosition);

// 初回だけマウスを中心に置く
if (isFirstMouseUpdate_) {

    SetCursorPos(centerScreenPosition.x, centerScreenPosition.y);

    previousMousePosition_.x = static_cast<float>(centerMousePosition.x);
    previousMousePosition_.y = static_cast<float>(centerMousePosition.y);

    isFirstMouseUpdate_ = false;
}

// 現在のマウス座標を取得
POINT currentMousePosition;
GetCursorPos(&currentMousePosition);

ScreenToClient(hwnd, &currentMousePosition);

// 中心からどれだけ動いたか
float mouseMoveX = static_cast<float>(currentMousePosition.x - centerMousePosition.x);
float mouseMoveY = static_cast<float>(currentMousePosition.y - centerMousePosition.y);

bool isMouseAimInput = false;

if (mouseMoveX != 0.0f || mouseMoveY != 0.0f) {
    isMouseAimInput = true;
}

// マウス感度
float mouseAimScreenSpeed = 1.0f;
float mouseAimWorldSpeed = 0.02f;

if (isMouseAimInput) {

    // 照準スプライトを動かす
    aimScreenPosition_.x += mouseMoveX * mouseAimScreenSpeed;
    aimScreenPosition_.y += mouseMoveY * mouseAimScreenSpeed;

    // ゲーム内照準も動かす
    aimPosition_.x += mouseMoveX * mouseAimWorldSpeed;
    aimPosition_.y += -mouseMoveY * mouseAimWorldSpeed;
}

// マウスを毎フレーム中央に戻す
SetCursorPos(centerScreenPosition.x, centerScreenPosition.y);

        // ==============================
        // 照準位置制限
        // ==============================

        if (aimPosition_.x > moveLimitX_) {
            aimPosition_.x = moveLimitX_;
        }

        if (aimPosition_.x < -moveLimitX_) {
            aimPosition_.x = -moveLimitX_;
        }

        if (aimPosition_.y > moveLimitY_) {
            aimPosition_.y = moveLimitY_;
        }

        if (aimPosition_.y < -moveLimitY_) {
            aimPosition_.y = -moveLimitY_;
        }

        // ==============================
        // プレイヤーが照準に遅れて追従
        // ==============================

        Vector3 difference;

        difference.x = aimPosition_.x - transform_.translate.x;

        difference.y = aimPosition_.y - transform_.translate.y;

        difference.z = 0.0f;

        transform_.translate.x += difference.x * aimFollowPower_;

        transform_.translate.y += difference.y * aimFollowPower_;

        // ==============================
        // 機体の傾き
        // ==============================

        transform_.rotate.z = -difference.x * 0.08f;

        transform_.rotate.x = difference.y * 0.08f;

        transform_.rotate.z += -velocity_.x * tiltPower_;

        transform_.rotate.x += velocity_.y * tiltPower_;

        // ==============================
        // 移動制限
        // ==============================

        if (transform_.translate.x > moveLimitX_) {

            transform_.translate.x = moveLimitX_;
        }

        if (transform_.translate.x < -moveLimitX_) {

            transform_.translate.x = -moveLimitX_;
        }

        if (transform_.translate.y > moveLimitY_) {

            transform_.translate.y = moveLimitY_;
        }

        if (transform_.translate.y < -moveLimitY_) {

            transform_.translate.y = -moveLimitY_;
        }
    }

    // オブジェクトのTransformを更新
    object_->SetScale(transform_.scale);
    object_->SetRotate(transform_.rotate);
    object_->SetTranslate(transform_.translate);

    // オブジェクトの更新
    object_->Update();
}
#pragma endregion

void Player::Draw()
{
    if (object_ == nullptr) {
        return;
    }

    object_->Draw();
}

void Player::SetCamera(Camera* camera)
{
    camera_ = camera;

    if (object_ != nullptr) {
        object_->SetCamera(camera_);
    }
}

void Player::SetDebugCameraController(DebugCameraController* debugCameraController)
{
    debugCameraController_ = debugCameraController;
}

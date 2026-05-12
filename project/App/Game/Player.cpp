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

    object_->SetScale(transform_.scale);
    object_->SetRotate(transform_.rotate);
    object_->SetTranslate(transform_.translate);
}

void Player::Update()
{
    if (object_ == nullptr) {
        return;
    }

    Input* input = Input::GetInstance();

    if (input == nullptr) {
        return;
    }

    if (debugCameraController_ != nullptr) {
        isDebugMode = debugCameraController_->GetDebugMode();
    }

    if (!isDebugMode) {

        Vector3 inputDirection = { 0.0f, 0.0f, 0.0f };

        if (input->IsKeyPressed(DIK_A)) {
            inputDirection.x -= 1.0f;
        }

        if (input->IsKeyPressed(DIK_D)) {
            inputDirection.x += 1.0f;
        }

        if (input->IsKeyPressed(DIK_W)) {
            inputDirection.z += 1.0f;
        }

        if (input->IsKeyPressed(DIK_S)) {
            inputDirection.z -= 1.0f;
        }

        velocity_.x += inputDirection.x * acceleration_;
        velocity_.z += inputDirection.z * acceleration_;

        if (velocity_.x > maxSpeed_) {
            velocity_.x = maxSpeed_;
        }

        if (velocity_.x < -maxSpeed_) {
            velocity_.x = -maxSpeed_;
        }

        if (velocity_.z > maxSpeed_) {
            velocity_.z = maxSpeed_;
        }

        if (velocity_.z < -maxSpeed_) {
            velocity_.z = -maxSpeed_;
        }

        velocity_.x *= deceleration_;
        velocity_.z *= deceleration_;

        transform_.translate.x += velocity_.x;
        transform_.translate.z += velocity_.z;

        if (transform_.translate.x > moveLimitX_) {
            transform_.translate.x = moveLimitX_;
            velocity_.x = 0.0f;
        }

        if (transform_.translate.x < -moveLimitX_) {
            transform_.translate.x = -moveLimitX_;
            velocity_.x = 0.0f;
        }

        if (transform_.translate.z > moveLimitZ_) {
            transform_.translate.z = moveLimitZ_;
            velocity_.z = 0.0f;
        }

        if (transform_.translate.z < -moveLimitZ_) {
            transform_.translate.z = -moveLimitZ_;
            velocity_.z = 0.0f;
        }

        POINT cursorPosition;
        GetCursorPos(&cursorPosition);

        ScreenToClient(WinApp::GetInstance()->GetHwnd(), &cursorPosition);

        float screenCenterX = static_cast<float>(WinApp::GetInstance()->kClientWidth) * 0.5f;

        float screenCenterY = static_cast<float>(WinApp::GetInstance()->kClientHeight) * 0.5f;

        float offsetX = static_cast<float>(cursorPosition.x) - screenCenterX;

        float offsetY = static_cast<float>(cursorPosition.y) - screenCenterY;

        float mouseRotatePower = 0.0015f;

        transform_.rotate.z = -offsetX * mouseRotatePower;

        transform_.rotate.x = offsetY * mouseRotatePower;

        transform_.rotate.z += -velocity_.x * tiltPower_;

        transform_.rotate.x += velocity_.z * tiltPower_;
    }

    object_->SetScale(transform_.scale);
    object_->SetRotate(transform_.rotate);
    object_->SetTranslate(transform_.translate);

    object_->Update();

#pragma region ImGuiによる環境マッピング操作パネル
    if (isDebugMode) {//debugModeの時だけ表示する
        ImGui::Begin("Environment Mapping Control");

        // --- AnimationActor ---
        static bool envMapEnabled = true;
        static float envMapStrength = 0.3f;

        ImGui::Checkbox("Player EnvMap", &envMapEnabled);
        ImGui::SliderFloat("Player Env Strength", &envMapStrength, 0.0f, 1.0f);

        if (object_) {
            object_->SetEnableEnvironmentMap(envMapEnabled);
            object_->SetEnvironmentMapStrength(envMapStrength);
        }

        ImGui::End();
    }
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

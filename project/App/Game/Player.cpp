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
        isDebugMode = debugCameraController_->IsDebugMode();
    }

    if (!isDebugMode) {
        if (input->IsKeyPressed(DIK_A)) {
            transform_.translate.x -= moveSpeed_;
        }

        if (input->IsKeyPressed(DIK_D)) {
            transform_.translate.x += moveSpeed_;
        }

        if (input->IsKeyPressed(DIK_W)) {
            transform_.translate.z += moveSpeed_;
        }

        if (input->IsKeyPressed(DIK_S)) {
            transform_.translate.z -= moveSpeed_;
        }
    }
    object_->SetScale(transform_.scale);
    object_->SetRotate(transform_.rotate);
    object_->SetTranslate(transform_.translate);
    object_->Update();
}

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
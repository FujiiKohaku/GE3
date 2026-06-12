#include "Player.h"
#include "../../Engine/3D/ModelManager.h"
#include "../../Engine/3D/Object3dManager.h"
#include "../../Engine/Input/Input.h"
#include "../../Engine/debugcamera/DebugCameraController.h"
#include <algorithm>
#include <cassert>

#include "../../externals/imgui/ImGuizmo.h"

#include "../../Engine/EditorManager/EditorManager.h"
void Player::Initialize(Model* model)
{
    assert(model != nullptr);

    object_ = std::make_unique<Object3d>();
    object_->Initialize(Object3dManager::GetInstance());
    object_->SetModel(model);

    if (camera_ != nullptr) {
        object_->SetCamera(camera_);
    }
    ModelManager::GetInstance()->Load("star.obj"); //
    bulletModel_ = ModelManager::GetInstance()->Load("star.obj");

    transform_.scale = { 1.0f, 1.0f, 1.0f };
    transform_.rotate = { 0.0f, 0.0f, 0.0f };
    transform_.translate = { 0.0f, -2.0f, 0.0f };
    aimScreenPosition_.x = WinApp::GetInstance()->kClientWidth / 2.0f;
    aimScreenPosition_.y = WinApp::GetInstance()->kClientHeight / 2.0f;
    object_->SetScale(transform_.scale);
    object_->SetRotate(transform_.rotate);
    object_->SetTranslate(transform_.translate);

    aimScreenPosition_.x = WinApp::GetInstance()->kClientWidth / 2.0f;
    aimScreenPosition_.y = WinApp::GetInstance()->kClientHeight / 2.0f;
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

    // デバッグカメラモードの取得
    if (debugCameraController_ != nullptr) {
        isDebugMode = debugCameraController_->GetDebugMode();
    }
    // plaerの移動速度
 
    transform_.translate.z += velocity_.z;
    // デバッグカメラモードでないときは、マウスで照準を動かし、キーボードでプレイヤーを動かす
    if (!isDebugMode) {
        UpdateMouseAim();
        UpdateKeyboardMove(input);
        ClampAimScreenPosition();
        ClampPlayerWorldPosition();
    }
    if (input->IsMouseTrigger(0)) {
        FireBullet();
    }

    // transform_.translate.z += 0.5f;
    //  transform反映
    ApplyTransform();
    // 弾更新と死んだ弾の削除
    UpdateBullets();
    // 死んだ弾の削除
    RemoveDeadBullets();

    object_->Update();
}
#pragma endregion

void Player::Draw()
{
    if (object_ == nullptr) {
        return;
    }
    for (std::unique_ptr<Bullet>& bullet : bullets_) {

        bullet->Draw();
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

// 照準の画面上の位置を制限する関数
void Player::ClampAimScreenPosition()
{
    float halfAimSize = 64.0f;

    // 左の制限64で固定
    if (aimScreenPosition_.x < halfAimSize) {
        aimScreenPosition_.x = halfAimSize;
    }

    // 右の制限はウィンドウ幅-64
    if (aimScreenPosition_.x > WinApp::GetInstance()->kClientWidth - halfAimSize) {
        aimScreenPosition_.x = WinApp::GetInstance()->kClientWidth - halfAimSize;
    }

    // 上の制限64で固定
    if (aimScreenPosition_.y < halfAimSize) {
        aimScreenPosition_.y = halfAimSize;
    }

    // 下の制限はウィンドウ高さ-64
    if (aimScreenPosition_.y > WinApp::GetInstance()->kClientHeight - halfAimSize) {
        aimScreenPosition_.y = WinApp::GetInstance()->kClientHeight - halfAimSize;
    }
}

// マウスで照準を動かす関数
void Player::UpdateMouseAim()
{
    HWND hwnd = WinApp::GetInstance()->GetHwnd();

    POINT mousePosition;
    GetCursorPos(&mousePosition);
    ScreenToClient(hwnd, &mousePosition);

    aimScreenPosition_.x = static_cast<float>(mousePosition.x);
    aimScreenPosition_.y = static_cast<float>(mousePosition.y);
}

void Player::ClampPlayerWorldPosition()
{
    if (transform_.translate.x > moveLimitX_ - playerClampMarginX_) {
        transform_.translate.x = moveLimitX_ - playerClampMarginX_;
    }

    if (transform_.translate.x < -moveLimitX_ + playerClampMarginX_) {
        transform_.translate.x = -moveLimitX_ + playerClampMarginX_;
    }

    if (transform_.translate.y > moveLimitY_ - playerClampMarginY_) {
        transform_.translate.y = moveLimitY_ - playerClampMarginY_;
    }

    if (transform_.translate.y < -moveLimitY_ + playerClampMarginY_) {
        transform_.translate.y = -moveLimitY_ + playerClampMarginY_;
    }
}

// 弾を発射する関数
void Player::FireBullet()
{
    if (bulletModel_ == nullptr) {
        return;
    }

    if (camera_ == nullptr) {
        return;
    }

    std::unique_ptr<Bullet> bullet = std::make_unique<Bullet>();

    bullet->Initialize(bulletModel_);
    bullet->SetCamera(camera_);

    Vector3 bulletPosition = transform_.translate;
    bulletPosition.y += bulletSpawnOffsetY_;
    bulletPosition.z += 0.0f;

    bullet->SetTranslate(bulletPosition);

    float mouseX = aimScreenPosition_.x;
    float mouseY = aimScreenPosition_.y;

    float screenWidth = static_cast<float>(WinApp::GetInstance()->kClientWidth);
    float screenHeight = static_cast<float>(WinApp::GetInstance()->kClientHeight);

    float ndcX = (2.0f * mouseX / screenWidth) - 1.0f;
    float ndcY = 1.0f - (2.0f * mouseY / screenHeight);

    Matrix4x4 inverseProjection = MatrixMath::Inverse(camera_->GetProjectionMatrix());
    Matrix4x4 inverseView = MatrixMath::Inverse(camera_->GetViewMatrix());

    Vector3 nearPoint = { ndcX, ndcY, 0.0f };
    Vector3 farPoint = { ndcX, ndcY, 1.0f };

    nearPoint = MatrixMath::Transform(nearPoint, inverseProjection);
    farPoint = MatrixMath::Transform(farPoint, inverseProjection);

    nearPoint = MatrixMath::Transform(nearPoint, inverseView);
    farPoint = MatrixMath::Transform(farPoint, inverseView);

    Vector3 bulletDirection = Normalize(farPoint - bulletPosition);

    Vector3 bulletVelocity;
    bulletVelocity.x = bulletDirection.x * bulletSpeed_+ velocity_.x;
    bulletVelocity.y = bulletDirection.y * bulletSpeed_+velocity_.y;
    bulletVelocity.z = bulletDirection.z * bulletSpeed_+velocity_.z;

    bullet->SetVelocity(bulletVelocity);

    bullets_.push_back(std::move(bullet));
}

// 弾更新
void Player::UpdateBullets()
{
    for (std::unique_ptr<Bullet>& bullet : bullets_) {
        bullet->Update();
    }
}

// 死んだ弾を削除する関数
void Player::RemoveDeadBullets()
{
    for (uint32_t i = 0; i < bullets_.size();) {
        if (!bullets_[i]->IsAlive()) {
            bullets_.erase(bullets_.begin() + i);
        } else {
            ++i;
        }
    }
}

// transform反映
void Player::ApplyTransform()
{
    object_->SetScale(transform_.scale);
    object_->SetRotate(transform_.rotate);
    object_->SetTranslate(transform_.translate);
}

void Player::UpdateKeyboardMove(Input* input)
{
    if (input->IsKeyPressed(DIK_A)) {
        transform_.translate.x -= moveSpeed_;
    }

    if (input->IsKeyPressed(DIK_D)) {
        transform_.translate.x += moveSpeed_;
    }

    if (input->IsKeyPressed(DIK_W)) {
        transform_.translate.y += moveSpeed_;
    }

    if (input->IsKeyPressed(DIK_S)) {
        transform_.translate.y -= moveSpeed_;
    }
}
#ifdef _DEBUG

void Player::DrawImGui()
{
    ImGui::Begin("Player");

    ImGui::Text("Position");

    ImGui::Text("X : %.2f", transform_.translate.x);
    ImGui::Text("Y : %.2f", transform_.translate.y);
    ImGui::Text("Z : %.2f", transform_.translate.z);

    ImGui::Separator();

    ImGui::Text("Velocity");

    ImGui::Text("X : %.2f", velocity_.x);
    ImGui::Text("Y : %.2f", velocity_.y);
    ImGui::Text("Z : %.2f", velocity_.z);

    ImGui::End();
}

#endif // _DEBUG

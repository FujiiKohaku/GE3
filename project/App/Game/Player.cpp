#include "Player.h"
#include "../../Engine/3D/ModelManager.h"
#include "../../Engine/3D/Object3dManager.h"
#include "../../Engine/Input/Input.h"
#include "../../Engine/debugcamera/DebugCameraController.h"
#include <algorithm>
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

    aimPosition_ = { 0.0f, 0.0f, 0.0f };
    velocity_ = { 0.0f, 0.0f, 0.0f };

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

    if (input->IsMousePressed(0)) {
        FireBullet();
    }

    if (debugCameraController_ != nullptr) {
        isDebugMode = debugCameraController_->GetDebugMode();
    }

    if (!isDebugMode) {
        UpdateKeyboardAim(input);
        UpdateMouseAim();

        ClampAimScreenPosition();
        ClampWorldAimPosition();

        FollowAimPosition();
        ClampPlayerWorldPosition();

        UpdateTilt();
    }

    ApplyTransform();

    UpdateBullets();
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

// キーボードで照準を動かす関数
void Player::UpdateKeyboardAim(Input* input)
{
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


        aimScreenPosition_.x += inputDirection.x * keyboardAimScreenSpeed_;
        aimScreenPosition_.y -= inputDirection.y * keyboardAimScreenSpeed_;
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
}

// マウスで照準を動かす関数
void Player::UpdateMouseAim()
{
    HWND hwnd = WinApp::GetInstance()->GetHwnd();

    POINT mousePosition;
    GetCursorPos(&mousePosition);
    ScreenToClient(hwnd, &mousePosition);



   float screenCenterX = static_cast<float>(WinApp::GetInstance()->kClientWidth) * 0.5f;

    float screenCenterY = static_cast<float>(WinApp::GetInstance()->kClientHeight) * 0.5f;

    float targetScreenX = static_cast<float>(mousePosition.x);
    float targetScreenY = static_cast<float>(mousePosition.y);

  
    aimScreenPosition_.x += (targetScreenX - aimScreenPosition_.x) * aimScreenFollowPower_;
    aimScreenPosition_.y += (targetScreenY - aimScreenPosition_.y) * aimScreenFollowPower_;

    float normalizedX = (aimScreenPosition_.x - screenCenterX) / screenCenterX;
    float normalizedY = (aimScreenPosition_.y - screenCenterY) / screenCenterY;

    float targetX = normalizedX * moveLimitX_;
    float targetY = -normalizedY * moveLimitY_;


    aimPosition_.x += (targetX - aimPosition_.x) * mouseAimFollowPower_;
    aimPosition_.y += (targetY - aimPosition_.y) * mouseAimFollowPower_;
}

// 照準のワールド上の位置を制限する処理
void Player::ClampWorldAimPosition()
{
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
}
// プレイヤーの位置を照準に追従させる処理
void Player::FollowAimPosition()
{
    Vector3 difference;
    difference.x = aimPosition_.x - transform_.translate.x;
    difference.y = aimPosition_.y - transform_.translate.y;
    difference.z = 0.0f;

    transform_.translate.x += difference.x * aimFollowPower_;
    transform_.translate.y += difference.y * aimFollowPower_;
}

void Player::UpdateTilt()
{
    float aimDirectionX = aimPosition_.x - transform_.translate.x;
    float aimDirectionY = aimPosition_.y - transform_.translate.y;

    Vector3 targetRotate;

    targetRotate.y = aimDirectionX * tiltAimPowerX_;
    targetRotate.x = -aimDirectionY * tiltAimPowerY_;
    targetRotate.z = -aimDirectionX * tiltRollPower_;
    targetRotate.z += -velocity_.x * tiltPower_;

    transform_.rotate.x += (targetRotate.x - transform_.rotate.x) * rotateFollowPower_;
    transform_.rotate.y += (targetRotate.y - transform_.rotate.y) * rotateFollowPower_;
    transform_.rotate.z += (targetRotate.z - transform_.rotate.z) * rotateFollowPower_;
}

void Player::ClampPlayerWorldPosition()
{

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
// 弾を発射する関数
void Player::FireBullet()
{
    if (bulletModel_ == nullptr) {
        return;
    }

    std::unique_ptr<Bullet> bullet = std::make_unique<Bullet>();

    bullet->Initialize(bulletModel_);
    bullet->SetCamera(camera_);

    Vector3 bulletPosition = transform_.translate;
    bulletPosition.y += 0.3f;
    bulletPosition.z += 4.0f;

    bullet->SetTranslate(bulletPosition);

    float screenWidth = static_cast<float>(WinApp::GetInstance()->kClientWidth);
    float screenHeight = static_cast<float>(WinApp::GetInstance()->kClientHeight);

    float screenCenterX = screenWidth * 0.5f;
    float screenCenterY = screenHeight * 0.5f;

    float normalizedAimX = (aimScreenPosition_.x - screenCenterX) / screenCenterX;
    float normalizedAimY = (aimScreenPosition_.y - screenCenterY) / screenCenterY;



    Vector3 bulletDirection;
    bulletDirection.x = normalizedAimX * bulletAimPowerX_;
    bulletDirection.y = -normalizedAimY * bulletAimPowerY_;
    bulletDirection.z = 1.0f;

    float length = std::sqrt(
        bulletDirection.x * bulletDirection.x + bulletDirection.y * bulletDirection.y + bulletDirection.z * bulletDirection.z);

    if (length > 0.0f) {
        bulletDirection.x /= length;
        bulletDirection.y /= length;
        bulletDirection.z /= length;
    }

    Vector3 bulletVelocity;
    bulletVelocity.x = bulletDirection.x * bulletSpeed_;
    bulletVelocity.y = bulletDirection.y * bulletSpeed_;
    bulletVelocity.z = bulletDirection.z * bulletSpeed_;

    bullet->SetVelocity(bulletVelocity);

    bullets_.push_back(std::move(bullet));
}
//弾更新
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

//transform反映
void Player::ApplyTransform()
{
    object_->SetScale(transform_.scale);
    object_->SetRotate(transform_.rotate);
    object_->SetTranslate(transform_.translate);
}

void Player::DrawImGui()
{
    ImGui::Begin("Player");

    ImGui::Text("Move");
    ImGui::DragFloat("Acceleration", &acceleration_, 0.001f, 0.0f, 1.0f);
    ImGui::DragFloat("Deceleration", &deceleration_, 0.001f, 0.0f, 1.0f);
    ImGui::DragFloat("Max Speed", &maxSpeed_, 0.001f, 0.0f, 5.0f);

    ImGui::Text("Aim");
    ImGui::DragFloat("Move Limit X", &moveLimitX_, 0.1f, 0.0f, 100.0f);
    ImGui::DragFloat("Move Limit Y", &moveLimitY_, 0.1f, 0.0f, 100.0f);
    ImGui::DragFloat("Aim Follow Power", &aimFollowPower_, 0.001f, 0.0f, 1.0f);
    ImGui::DragFloat("Aim Screen Follow Power", &aimScreenFollowPower_, 0.001f, 0.0f, 1.0f);
    ImGui::DragFloat("Mouse Aim Follow Power", &mouseAimFollowPower_, 0.001f, 0.0f, 1.0f);
    ImGui::DragFloat("Keyboard Aim Screen Speed", &keyboardAimScreenSpeed_, 0.1f, 0.0f, 100.0f);

    ImGui::Text("Bullet");
    ImGui::DragFloat("Bullet Spawn Offset Y", &bulletSpawnOffsetY_, 0.1f, -10.0f, 10.0f);
    ImGui::DragFloat("Bullet Spawn Offset Z", &bulletSpawnOffsetZ_, 0.1f, -10.0f, 30.0f);
    ImGui::DragFloat("Bullet Aim Power X", &bulletAimPowerX_, 0.01f, 0.0f, 5.0f);
    ImGui::DragFloat("Bullet Aim Power Y", &bulletAimPowerY_, 0.01f, 0.0f, 5.0f);
    ImGui::DragFloat("Bullet Speed", &bulletSpeed_, 0.1f, 0.0f, 20.0f);

    ImGui::Text("Tilt");
    ImGui::DragFloat("Tilt Power", &tiltPower_, 0.01f, 0.0f, 5.0f);
    ImGui::DragFloat("Tilt Aim Power X", &tiltAimPowerX_, 0.001f, 0.0f, 1.0f);
    ImGui::DragFloat("Tilt Aim Power Y", &tiltAimPowerY_, 0.001f, 0.0f, 1.0f);
    ImGui::DragFloat("Tilt Roll Power", &tiltRollPower_, 0.001f, 0.0f, 1.0f);
    ImGui::DragFloat("Rotate Follow Power", &rotateFollowPower_, 0.001f, 0.0f, 1.0f);
    ImGui::Separator();

    if (ImGui::Button("Reset Player Params")) {
        ResetParameters();
    }
    ImGui::End();
}

void Player::ResetParameters()
{
    acceleration_ = 0.02f;
    deceleration_ = 0.85f;
    maxSpeed_ = 0.18f;

    moveLimitX_ = 22.0f;
    moveLimitY_ = 9.0f;

    tiltPower_ = 0.25f;

    aimFollowPower_ = 0.01f;
    aimScreenFollowPower_ = 1.0f;
    mouseAimFollowPower_ = 0.08f;

    keyboardAimScreenSpeed_ = 8.0f;

    bulletSpawnOffsetY_ = 0.3f;
    bulletSpawnOffsetZ_ = 4.0f;

    bulletAimPowerX_ = 0.8f;
    bulletAimPowerY_ = 0.4f;

    bulletSpeed_ = 2.5f;

    tiltAimPowerX_ = 0.04f;
    tiltAimPowerY_ = 0.04f;
    tiltRollPower_ = 0.03f;

    rotateFollowPower_ = 0.003f;
}
// 選択中の武器を切り替え、発射・発熱・ロックオンと生成済みのプレイヤー弾を管理する。
#include "App/Game/Player/Weapon/PlayerWeaponController.h"
#include "App/Game/Player/Player.h"
#include <functional>
#include "Engine/3D/ModelManager.h"
#include "Engine/Input/Input.h"
#include "Engine/Time/TimeManager.h"
#include "Engine/CollisionManager/CollisionManager.h"
#include "Engine/Effect/EffectManager.h"
#include "Engine/Debug/DebugRenderer.h"
#include <algorithm>

// 機体の姿勢と状態を参照する。参照先のPlayerは、この管理クラスより長く生存する必要がある。
PlayerWeaponController::PlayerWeaponController(const Player& player)
    : transform_(player.transform_), player_(player)
{
}

// 発射回数をリセットし、プレイヤー弾の表示に使うモデルを読み込む。
void PlayerWeaponController::Initialize()
{
    shotsFired_ = 0;
    bulletModel_ = ModelManager::GetInstance()->Load("Enemy/Bullet/LowPolySphere/lowPolySphere.obj");
}

// 新しいカメラを記録し、すでに発射済みの弾にも同じカメラを設定する。
void PlayerWeaponController::SetCamera(Camera* camera)
{
    camera_ = camera;
    for (const auto& bullet : bullets_) {
        bullet->SetCamera(camera);
    }
}

// 発熱・ロック・機体情報と発射用の呼び出し先を、武器が使う情報へまとめる。
PlayerWeaponContext PlayerWeaponController::MakeWeaponContext(const Camera* activeCamera)
{
    return { weaponHeat_, homingLock_, missileFireCooldownSeconds_, camera_,
        transform_.translate, player_.GetFlightForward(), player_.GetAimScreenPosition(),
        !player_.IsDead() && bulletModel_ != nullptr && camera_ != nullptr,
        std::bind(&PlayerWeaponController::FireCurrentCameraBullet, this),
        std::bind(&PlayerWeaponController::EmitBulletForCamera, this, activeCamera, std::placeholders::_1) };
}

// 有効なカメラがある場合だけ、現在の武器へ発射を要求する。
bool PlayerWeaponController::FireCurrentCameraBullet()
{
    if (camera_ == nullptr) { return false; }
    return FireBullet(*camera_);
}

// 発射用カメラを選び、指定したロック対象への弾を生成する。
void PlayerWeaponController::EmitBulletForCamera(const Camera* activeCamera, BaseEnemy* target)
{
    const Camera* shotCamera = activeCamera;
    if (shotCamera == nullptr) { shotCamera = camera_; }
    if (shotCamera != nullptr) { FireSingleBullet(*shotCamera, target); }
}

// 武器切替・冷却・ミサイル間隔を更新し、選択中の武器へ射撃入力を渡す。
void PlayerWeaponController::UpdateInput(Input* input, bool isDebugMode)
{
    if (input->IsPrimaryFireBlocked()) homingLock_.Reset();
    if (!isDebugMode) UpdateWeaponSwitch(input);
    const float dt = TimeManager::GetInstance()->GetDeltaTime();
    missileFireCooldownSeconds_ = (std::min)(kMissileFireIntervalSeconds, missileFireCooldownSeconds_ + dt);
    BasePlayerWeapon& weapon = GetCurrentWeapon();
    if (isDebugMode || !weapon.IsHoming()) homingLock_.Reset();
    auto context = MakeWeaponContext();
    context.canFire = context.canFire && !isDebugMode;
    // ミサイル選択中にも共通の熱ゲージは冷却を続ける。
    weaponHeat_.Update(dt, context.canFire && weapon.UsesHeat() && input->IsPrimaryFirePressed());
    weapon.Update(context, input->IsPrimaryFirePressed(), input->IsPrimaryFireTrigger(), dt);
}
// 追尾ミサイル選択中だけ、ロック表示に必要な敵の位置を書き出す。
void PlayerWeaponController::GetHomingLockPositions(std::vector<Vector3>& positions) const
{
    positions.clear();
    if (IsHomingMissileSelected()) {
        homingLock_.GetPositions(positions);
    }
}

void PlayerWeaponController::DrawBullets()
{
    for (const auto& bullet : bullets_) {
        bullet->Draw();
    }
}

// 描画対象をレイトレーシング用のシーンへ登録する。
void PlayerWeaponController::SubmitRaytracing(DxrRenderer& renderer) {
    for (const auto& bullet : bullets_) {
        if (bullet) { bullet->SubmitRaytracing(renderer); }
    }
}

// 現在の武器へ発射を要求し、その武器固有の過熱やクールダウン判定を使う。
bool PlayerWeaponController::FireBullet(const Camera& activeCamera)
{
    auto context = MakeWeaponContext(&activeCamera);
    return GetCurrentWeapon().Fire(context);
}
// 武器に弾を生成させ、熱量の色・銃口位置・照準方向・機体速度を反映して所有する。
void PlayerWeaponController::FireSingleBullet(const Camera& activeCamera, BaseEnemy* homingTarget)
{

    float shotSpeed = bulletSpeed_;
    auto context = MakeWeaponContext(&activeCamera);
    std::unique_ptr<PlayerBullet> bullet = GetCurrentWeapon().CreateBullet(
        bulletModel_, context, homingTarget, shotSpeed);
    constexpr Vector4 kColdBulletColor = { 0.12f, 0.48f, 1.0f, 1.0f };
    constexpr Vector4 kHotBulletColor = { 0.85f, 0.015f, 0.008f, 1.0f };
    const float heatRatio = std::clamp(weaponHeat_.GetHeatRatio(), 0.0f, 1.0f);
    const Vector4 bulletColor = {
        kColdBulletColor.x + (kHotBulletColor.x - kColdBulletColor.x) * heatRatio,
        kColdBulletColor.y + (kHotBulletColor.y - kColdBulletColor.y) * heatRatio,
        kColdBulletColor.z + (kHotBulletColor.z - kColdBulletColor.z) * heatRatio,
        1.0f
    };
    bullet->SetColor(bulletColor);
    bullet->SetCamera(camera_);
    Vector3 muzzlePosition = CalculateMuzzlePosition();
    EffectManager::GetInstance()->PlayEffect("ShotBullet", muzzlePosition);

    bullet->SetTranslate(muzzlePosition);

    Ray aimRay {};
    // 発射処理で、画面上の照準を3D空間の方向へ変換する。
    player_.CreateAimRay(aimRay, activeCamera);

    // 銃口から弾を向ける位置を、補間済みの照準距離から求める。
    Vector3 aimPoint = player_.ResolveAimPoint(aimRay);


#ifdef _DEBUG
    shouldDrawDebugLines_ = true;
    debugAimRayOrigin_ = aimRay.origin;
    debugAimPoint_ = aimPoint;
    debugMuzzlePosition_ = muzzlePosition;

    Ray drawRay {};
    // デバッグ描画用に照準レイと狙い位置を求め、発射方向との関係を表示する。
    player_.CreateAimRay(drawRay, *camera_);
    Vector3 drawAimPoint = player_.ResolveAimPoint(drawRay);
    debugDrawRayOrigin_ = drawRay.origin;
    debugDrawAimPoint_ = drawAimPoint;
#endif

    Vector3 bulletDirection = Normalize(aimPoint - muzzlePosition);
    Vector3 worldPlayerVelocity = player_.GetWorldVelocity();

    Vector3 bulletVelocity;
    bulletVelocity.x = bulletDirection.x * shotSpeed + worldPlayerVelocity.x;
    bulletVelocity.y = bulletDirection.y * shotSpeed + worldPlayerVelocity.y;
    bulletVelocity.z = bulletDirection.z * shotSpeed + worldPlayerVelocity.z;

    bullet->SetVelocity(bulletVelocity);

    // 発射後の弾は管理側が所有し、以後の更新・衝突判定・削除の対象にする。
    bullets_.push_back(std::move(bullet));
    ++shotsFired_;
}

// 機体に対する銃口の相対位置を、現在の姿勢でワールド位置へ変換する。
Vector3 PlayerWeaponController::CalculateMuzzlePosition() const
{
    Matrix4x4 worldMatrix = MatrixMath::MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
    Vector3 localMuzzle = { 0.0f, bulletSpawnOffsetY_, bulletSpawnOffsetZ_ };
    return MatrixMath::Transform(localMuzzle, worldMatrix);
}

// ホイール・方向キー・数字キーの入力を武器の選択番号へ反映する。
void PlayerWeaponController::UpdateWeaponSwitch(Input* input)
{
    if (input->GetMouseWheel() > 0 || input->IsGamepadButtonTrigger(XINPUT_GAMEPAD_DPAD_RIGHT)) {
        currentWeapon_ = (currentWeapon_ + 1) % kWeaponCount;
    }

    if (input->GetMouseWheel() < 0 || input->IsGamepadButtonTrigger(XINPUT_GAMEPAD_DPAD_LEFT)) {
        currentWeapon_ = (currentWeapon_ + kWeaponCount - 1) % kWeaponCount;
    }

    if (input->IsKeyTrigger(DIK_1)) currentWeapon_ = kWeaponNormalBullet;
    if (input->IsKeyTrigger(DIK_2)) currentWeapon_ = kWeaponMissileBullet;
    if (input->IsKeyTrigger(DIK_3)) currentWeapon_ = kWeaponHomingMissile;
    if (input->IsKeyTrigger(DIK_4)) currentWeapon_ = kWeaponMinigun;
}

const char* PlayerWeaponController::GetCurrentWeaponName() const
{
    return GetCurrentWeapon().GetName();
}
// 所有しているプレイヤー弾を一フレームずつ更新する。
void PlayerWeaponController::UpdateBullets()
{
    for (std::unique_ptr<PlayerBullet>& bullet : bullets_) {
        bullet->Update();
    }
}

// 命中や寿命で消滅した弾を一覧から外し、所有していたオブジェクトを破棄する。
void PlayerWeaponController::RemoveDeadBullets()
{
    for (uint32_t i = 0; i < bullets_.size();) {
        if (!bullets_[i]->IsAlive()) {
            bullets_.erase(bullets_.begin() + i);
        } else {
            ++i;
        }
    }

}

#ifdef _DEBUG
// 照準レイと銃口からの射線を線で表示し、狙いのずれを確認できるようにする。
void PlayerWeaponController::DrawAimDebugLines()
{
    if (shouldDrawDebugLines_) {
        // AimCameraから飛ぶRay (青)
        DebugRenderer::GetInstance()->AddLine(debugAimRayOrigin_, debugAimPoint_, { 0.0f, 0.0f, 1.0f, 1.0f }, 3.0f);

        // Playerのマズルから飛ぶ実際の弾の進行方向 (赤)
        DebugRenderer::GetInstance()->AddLine(debugMuzzlePosition_, debugAimPoint_, { 1.0f, 0.0f, 0.0f, 1.0f }, 3.0f);

        // 描画用Cameraから飛ぶRay (黄色 - デバッグ比較専用)
        DebugRenderer::GetInstance()->AddLine(debugDrawRayOrigin_, debugDrawAimPoint_, { 1.0f, 1.0f, 0.0f, 1.0f }, 3.0f);
    }
}
#endif

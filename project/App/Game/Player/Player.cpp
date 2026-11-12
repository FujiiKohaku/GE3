#include "App/Game/Player/Player.h"
#include "Engine/Effect/EffectManager.h"
#include <cmath>
#include "App/Game/Common/CombatTarget.h"
#include "App/Game/Common/GameTiming.h"
#include "Engine/Time/TimeManager.h"
#include "App/Game/Enemy/BaseEnemy.h"
#include <algorithm>
#include "Engine/audio/SoundManager.h"
#include "Engine/Camera/Camera.h"
#include "Engine/3D/ModelManager.h"
#include "Engine/Input/Input.h"
#include "Engine/CollisionManager/CollisionManager.h"
#include "Engine/Debug/DebugRenderer.h"
#include "Engine/Winapp/WinApp.h"
#include <numbers>
#include "Engine/math/MatrixMath.h"
#include "Engine/3D/Object3dManager.h"
#include <cassert>

// PlayerBullet
// プレイヤー弾に共通する敵への命中処理を定義し、敵弾とは別の型として扱う。

// 弾種固有の命中演出を追加するための共通の呼び出し口。標準では何も行わない。
void PlayerBullet::OnHitEnemy(const Vector3& position)
{
}

// NormalBullet
// プレイヤーの通常弾の描画設定と、敵への命中時のエフェクトを扱う。
// 通常弾の色・シェーダー・霧や影の設定を適用し、機体から見やすい弾にする。
void NormalBullet::Initialize(Model* model)
{
    // ここでは、BaseBulletのInitialize関数を呼び出して、モデルの設定やオブジェクトの初期化を行います。
    BaseBullet::Initialize(model);
    SetEnableLighting(false);
    SetColor({ 0.12f, 0.48f, 1.0f, 1.0f });
    SetPixelShaderPath("resources/Shaders/Object3D/PlayerBullet/Render.PS.hlsl");
    SetFogEnabled(false);
    SetTransparent(true);
    SetTransparentDepthWrite(false);
    SetCastShadow(false);
    transform_.scale = { 0.3f, 0.3f, 0.3f };
    transform_.rotate = { 0.0f, 0.0f, 0.0f };
    transform_.translate = { 0.0f, 0.0f, 0.0f };
}

// 共通の移動と寿命処理を使い、通常弾を進める。
void NormalBullet::Update()
{
    BaseBullet::Update();
}
// 通常弾の命中位置へヒットエフェクトを出す。
void NormalBullet::OnHitEnemy(const Vector3& position)
{
    EffectManager::GetInstance()->PlayEffect(
        "NormalBulletHit",
        position);

    EffectManager::GetInstance()->PlayEffect(
        "NormalBulletImpactFlash",
        position);
}

// MissileBullet
// ミサイルの描画・威力と航跡エフェクトを扱う。消滅時には航跡も停止する。


// ミサイルの見た目と威力を設定し、追従する航跡エフェクトを開始する。
void MissileBullet::Initialize(Model* model)
{
    PlayerBullet::Initialize(model);
    SetEnableLighting(false);
    SetColor({ 0.12f, 0.48f, 1.0f, 1.0f });
    SetPixelShaderPath("resources/Shaders/Object3D/PlayerBullet/Render.PS.hlsl");
    SetFogEnabled(false);
    SetTransparent(true);
    SetTransparentDepthWrite(false);
    SetCastShadow(false);

    SetDamage(10);

    transform_.scale = {
        scale_,
        scale_,
        scale_
    };

    CreateTrailEffect();
}

// 移動と寿命を更新し、航跡を追従させる。消滅した場合は航跡を停止する。
void MissileBullet::Update()
{
    PlayerBullet::Update();

    UpdateTrailPosition();

    if (!IsAlive()) {
        StopTrailEffect();
    }
}

// 本体の位置を設定し、航跡の追従先も同じ位置へ更新する。
void MissileBullet::SetTranslate(const Vector3& translate)
{
    PlayerBullet::SetTranslate(translate);
    UpdateTrailPosition();
}

// 弾を消滅扱いにし、その場に航跡だけが残り続けないよう停止する。
void MissileBullet::SetDead()
{
    PlayerBullet::SetDead();

    StopTrailEffect();
}

// 命中位置にミサイル用の演出を出す。
void MissileBullet::OnHitEnemy(const Vector3& position)
{
    EffectManager::GetInstance()->PlayEffect(
        "MissileExplosion",
        position);
    EffectManager::GetInstance()->PlayEffect(
        "MissileExplosionParticles",
        position);
    EffectManager::GetInstance()->PlayEffect(
        "MissileExplosionRing",
        position);
    EffectManager::GetInstance()->PlayEffect(
        "MissileExplosionFlash",
        position);
}

PlayerBulletExplosionSettings MissileBullet::GetExplosionSettingsFromConfig(const nlohmann::json& config)
{
    if (!config.is_object() || !config.contains("Particle") || !config.at("Particle").is_object() ||
        !config.contains("Render") || !config.at("Render").is_object()) { return {}; }
    const auto& render = config.at("Render");
    if (!render.contains("MeshType") || render.at("MeshType") != "Sphere") { return {}; }
    if (render.contains("Type") && render.at("Type") != "Mesh") { return {}; }
    const auto& particle = config.at("Particle");
    if (!particle.contains("EndScale") || !particle.at("EndScale").is_number()) { return {}; }
    const float radius = particle.at("EndScale").get<float>();
    if (!std::isfinite(radius) || radius <= 0.0f) { return {}; }
    return { radius, kExplosionDamage };
}

PlayerBulletExplosionSettings MissileBullet::GetExplosionSettings() const
{
    nlohmann::json config;
    std::string error;
    // 読み込まれていない演出の半径を推測して、見えない爆風を発生させない。
    if (!EffectManager::GetInstance()->GetEffectConfig("MissileExplosion", config, error)) { return {}; }
    return GetExplosionSettingsFromConfig(config);
}

// ミサイルの位置を追いかけるループエフェクトを生成する。
void MissileBullet::CreateTrailEffect()
{
    trailPosition_ = std::make_shared<Vector3>(transform_.translate);

    UpdateTrailPosition();

    std::shared_ptr<Vector3> trailPosition = trailPosition_;

    trailEffectHandle_ = EffectManager::GetInstance()->AttachEffect(
        "MissileTrail",
        [trailPosition]() {
            return *trailPosition;
        });
}

// 弾の位置に航跡の発生位置を追従させる。
void MissileBullet::UpdateTrailPosition()
{
    if (!trailPosition_) {
        return;
    }

    Vector3 trailPosition = transform_.translate;

    float velocityLength = std::sqrt(
        velocity_.x * velocity_.x + velocity_.y * velocity_.y + velocity_.z * velocity_.z);

    if (velocityLength > 0.001f) {
        trailPosition.x -= velocity_.x / velocityLength * 2.0f;
        trailPosition.y -= velocity_.y / velocityLength * 2.0f;
        trailPosition.z -= velocity_.z / velocityLength * 2.0f;
    }

    *trailPosition_ = trailPosition;
}

// 再生中の航跡を停止し、エフェクトのハンドルを無効化する。
void MissileBullet::StopTrailEffect()
{
    if (trailEffectHandle_ == kInvalidEffectHandle) {
        return;
    }

    EffectManager::GetInstance()->StopEffect(
        trailEffectHandle_);

    trailEffectHandle_ = kInvalidEffectHandle;
}

// HomingMissileBullet
// 生存しているロック対象へ進行方向を曲げ、目標を通り過ぎた回数も管理するミサイル。

void HomingMissileBullet::OnHitEnemy(const Vector3& position)
{
    EffectManager::GetInstance()->PlayEffect("NormalBulletImpactFlash", position);
}

// ロック対象と候補一覧を記録する。候補一覧は弱参照で保持し、寿命を引き延ばさない。
void HomingMissileBullet::SetTarget(CombatTarget* target, const std::shared_ptr<const std::vector<CombatTarget*>>& activeTargets)
{
    target_ = target;
    activeTargets_ = activeTargets;
    targetPassCount_ = 0;
}

// 対象の生存を確認して進行方向を曲げ、接近時の補正と通過回数によって追尾を制御する。
void HomingMissileBullet::Move()
{
    const std::shared_ptr<const std::vector<CombatTarget*>> activeTargets = activeTargets_.lock();
    const bool targetIsActive = activeTargets != nullptr &&
        std::find(activeTargets->begin(), activeTargets->end(), target_) != activeTargets->end();

    if (targetIsActive && target_ != nullptr && !target_->IsDead()) {
        const Vector3 targetPosition = target_->GetPosition();
        const Vector3 toTarget = targetPosition - transform_.translate;
        const float distanceSquared = Dot(toTarget, toTarget);
        if (distanceSquared > 0.0001f) {
            const float speed = std::sqrt(Dot(velocity_, velocity_));
            const Vector3 desiredVelocity = Normalize(toTarget) * speed;
            const float frameScale = TimeManager::GetInstance()->GetDeltaTime() * 60.0f;
            const float distance = std::sqrt(distanceSquared);
            const float closeRatio = std::clamp(
                1.0f - distance / kCloseHomingDistance,
                0.0f,
                1.0f);
            const float steeringStrength =
                homingStrength_ +
                (kCloseHomingStrength - homingStrength_) * closeRatio;
            const float blend = GameTiming::CalculateFollowBlend(steeringStrength, TimeManager::GetInstance()->GetDeltaTime());
            velocity_ = Normalize(velocity_ * (1.0f - blend) + desiredVelocity * blend) * speed;

            const Vector3 frameMovement = velocity_ * frameScale;
            const Vector3 afterMoveToTarget = toTarget - frameMovement;
            const bool passesTargetThisFrame =
                Dot(toTarget, velocity_) > 0.0f &&
                Dot(afterMoveToTarget, velocity_) <= 0.0f;
            if (passesTargetThisFrame) {
                const float movementSquared = Dot(frameMovement, frameMovement);
                float closestDistanceSquared = distanceSquared;
                if (movementSquared > 0.0001f) {
                    const float closestTime = std::clamp(
                        Dot(toTarget, frameMovement) / movementSquared,
                        0.0f,
                        1.0f);
                    const Vector3 closestOffset =
                        toTarget - frameMovement * closestTime;
                    closestDistanceSquared = Dot(closestOffset, closestOffset);
                }

                if (closestDistanceSquared <=
                    kTargetSnapDistance * kTargetSnapDistance) {
                    transform_.translate = targetPosition;
                    return;
                }

                ++targetPassCount_;
                if (targetPassCount_ >= kMaxTargetPassCount) {
                    SetDead();
                    return;
                }
            }
        }
    } else {
        target_ = nullptr;
    }

    MissileBullet::Move();
}

// PlayerMovement
namespace {
// 中央付近の小さな入力を無視し、意図しない操舵を防ぐ。
float ApplySteeringDeadZone(float value)
{
    constexpr float kDeadZone = 0.05f;
    const float magnitude = std::abs(value);
    if (magnitude <= kDeadZone) {
        return 0.0f;
    }
    // デッドゾーンの外側を0～1へ引き伸ばし、元の左右・上下の符号を戻す。
    return std::copysign((magnitude - kDeadZone) / (1.0f - kDeadZone), value);
}
}

void PlayerMovement::InitializeMovement(const EulerTransform& transform)
{
    railBasePosition_ = transform.translate;
    railOffset_ = {};
}

void PlayerMovement::SynchronizePosition(const EulerTransform& transform)
{
    // 現在のオフセットを維持し、次のレール位置計算でも設定位置を再現する。
    railBasePosition_ = transform.translate - railRight_ * railOffset_.x -
        railUp_ * railOffset_.y - railForward_ * railOffset_.z;
}

void PlayerMovement::SynchronizeOrientation(const EulerTransform& transform)
{
    if (!isAllRangeMode_) {
        return;
    }
    flightYaw_ = -transform.rotate.y;
    flightPitch_ = -transform.rotate.x;
    railForward_ = { std::sin(flightYaw_) * std::cos(flightPitch_),
        std::sin(flightPitch_), std::cos(flightYaw_) * std::cos(flightPitch_) };
    railRight_ = Normalize(Cross(Vector3{ 0.0f, 1.0f, 0.0f }, railForward_));
    railUp_ = Normalize(Cross(railForward_, railRight_));
}

void PlayerMovement::ApplyRailPosition(EulerTransform& transform)
{
    if (!isAllRangeMode_) {
        transform.translate = CalculateRailWorldPosition(railOffset_);
    }
}

void PlayerMovement::StopMovement()
{
    // 死亡時に操作状態を解除する。速度や位置を0にする処理ではない。
    isBoosting_ = false;
}

Vector3 PlayerMovement::GetAutomaticMovementVelocity() const
{
    if (isAllRangeMode_) {
        return railForward_ * velocity_.z;
    }
    return railForward_ * normalMaxSpeed_;
}

void PlayerMovement::SetRailFrame(const Vector3& railBasePosition,
    const Vector3& railRight, const Vector3& railUp, const Vector3& railForward)
{
    railBasePosition_ = railBasePosition;
    railRight_ = railRight;
    railUp_ = railUp;
    railForward_ = railForward;
}

void PlayerMovement::ApplyRailAreaForce(const Vector3& force, float deltaTimeSeconds)
{
    const float referenceStep = GameTiming::CalculateReferenceStep(deltaTimeSeconds);
    railOffset_.x += force.x * referenceStep;
    railOffset_.y += force.y * referenceStep;
    railOffset_ = ClampRailOffsetToScreen(railOffset_);
}

bool PlayerMovement::UpdateBoost(bool isBraking, bool shouldBoost)
{
    constexpr float kBrakeSpeedMultiplier = 0.5f;
    const bool wasBoosting = isBoosting_;
    isBoosting_ = shouldBoost && !isBraking;
    velocity_.z = normalMaxSpeed_;
    moveSpeed_ = normalAcceleration_;
    if (isBraking) {
        velocity_.z = normalMaxSpeed_ * kBrakeSpeedMultiplier;
    } else if (isBoosting_) {
        velocity_.z = boostMaxSpeed_;
        moveSpeed_ = boostAcceleration_;
    }
    return isBoosting_ && !wasBoosting;
}

void PlayerMovement::EnableAllRangeMode(const EulerTransform& transform, float areaRadius, float minHeight, float maxHeight)
{
    isAllRangeMode_ = true;
    flightAreaRadius_ = areaRadius;
    flightMinHeight_ = minHeight;
    flightMaxHeight_ = maxHeight;
    // 切り替え前の機体姿勢を引き継ぎ、飛行方向と直交する左右・上下の軸を作る。
    flightYaw_ = -transform.rotate.y;
    flightPitch_ = -transform.rotate.x;
    railForward_ = {
        std::sin(flightYaw_) * std::cos(flightPitch_),
        std::sin(flightPitch_),
        std::cos(flightYaw_) * std::cos(flightPitch_)
    };
    railRight_ = Normalize(Cross(Vector3 { 0.0f, 1.0f, 0.0f }, railForward_));
    railUp_ = Normalize(Cross(railForward_, railRight_));
    isReturningToFlightArea_ = false;
    railOffset_ = {};
    ResetSteering();
}

void PlayerMovement::UpdateAllRangeMove(EulerTransform& transform, Vector2 steering, float deltaTimeSeconds)
{
    UpdateFlightOrientation(steering, deltaTimeSeconds);
    UpdateFlightAreaReturn(transform, steering, deltaTimeSeconds);
    UpdateFlightHeight(transform, deltaTimeSeconds);
    UpdateFlightPosition(transform, deltaTimeSeconds);
}

void PlayerMovement::UpdateFlightOrientation(const Vector2& steering, float deltaTimeSeconds)
{
    constexpr float kYawSpeed = 1.35f;
    constexpr float kPitchSpeed = 0.85f;
    constexpr float kMaxPitch = 0.95f;
    flightYaw_ += steering.x * kYawSpeed * deltaTimeSeconds;
    flightPitch_ = std::clamp(
        flightPitch_ + steering.y * kPitchSpeed * deltaTimeSeconds, -kMaxPitch, kMaxPitch);
}

void PlayerMovement::UpdateFlightAreaReturn(const EulerTransform& transform, Vector2& steering, float deltaTimeSeconds)
{
    constexpr float kYawSpeed = 1.35f;
    // 追従カメラも範囲内に収まるよう、機体が端に達する前に内側への旋回を始める。
    const float horizontalDistance = std::sqrt(
        transform.translate.x * transform.translate.x +
        transform.translate.z * transform.translate.z);
    // 復帰開始と終了の距離に差を設け、境界付近で状態が頻繁に切り替わるのを防ぐ。
    if (horizontalDistance > flightAreaRadius_ - 90.0f) {
        isReturningToFlightArea_ = true;
    } else if (horizontalDistance < flightAreaRadius_ - 150.0f) {
        isReturningToFlightArea_ = false;
    }
    if (isReturningToFlightArea_) {
        // 先に加えた手動の左右旋回を取り消し、原点方向へ自動旋回させる。
        flightYaw_ -= steering.x * kYawSpeed * deltaTimeSeconds;
        const float inwardYaw = std::atan2(-transform.translate.x, -transform.translate.z);
        const float yawDifference = std::remainder(
            inwardYaw - flightYaw_, 2.0f * std::numbers::pi_v<float>);
        flightYaw_ += std::clamp(yawDifference, -2.0f * deltaTimeSeconds, 2.0f * deltaTimeSeconds);
        steering.x = std::clamp(yawDifference, -1.0f, 1.0f);
    }
    flightYaw_ = std::remainder(flightYaw_, 2.0f * std::numbers::pi_v<float>);
}

void PlayerMovement::UpdateFlightHeight(const EulerTransform& transform, float deltaTimeSeconds)
{
    // 高度の端へ向かう場合だけ、反対向きの緩やかなピッチへ近づける。
    if (transform.translate.y < flightMinHeight_ + 12.0f && flightPitch_ < 0.0f) {
        flightPitch_ += (0.25f - flightPitch_) * (1.0f - std::exp(-4.0f * deltaTimeSeconds));
    }
    if (transform.translate.y > flightMaxHeight_ - 12.0f && flightPitch_ > 0.0f) {
        flightPitch_ += (-0.25f - flightPitch_) * (1.0f - std::exp(-4.0f * deltaTimeSeconds));
    }
}

void PlayerMovement::UpdateFlightPosition(EulerTransform& transform, float deltaTimeSeconds)
{
    railForward_ = {
        std::sin(flightYaw_) * std::cos(flightPitch_),
        std::sin(flightPitch_),
        std::cos(flightYaw_) * std::cos(flightPitch_)
    };
    railRight_ = Normalize(Cross(Vector3 { 0.0f, 1.0f, 0.0f }, railForward_));
    railUp_ = Normalize(Cross(railForward_, railRight_));
    // 60FPS基準の前進量を経過秒数に換算し、フレームレートに応じて移動させる。
    transform.translate += railForward_ * (velocity_.z * 60.0f * deltaTimeSeconds);
    const float distanceAfterMove = std::sqrt(
        transform.translate.x * transform.translate.x +
        transform.translate.z * transform.translate.z);
    if (distanceAfterMove > flightAreaRadius_) {
        // 自動復帰でも範囲外へ出た場合は、水平位置を円の境界へ戻す。
        const float correction = flightAreaRadius_ / distanceAfterMove;
        transform.translate.x *= correction;
        transform.translate.z *= correction;
    }
    transform.translate.y = std::clamp(
        transform.translate.y, flightMinHeight_, flightMaxHeight_);
    railBasePosition_ = transform.translate;
    transform.rotate.x = -flightPitch_;
    transform.rotate.y = -flightYaw_;
}

void PlayerMovement::SetMouseSensitivity(float sensitivity)
{
    // 極端に小さい・大きい感度にならないよう、設定値を制限する。
    mouseSensitivity_ = std::clamp(sensitivity, 0.5f, 2.0f);
}

void PlayerMovement::UpdateSteering(const Vector2& aimPosition, const Vector2& screenSizePixels,
    bool isAllRangeMode, float deltaTimeSeconds, bool isGamepad)
{
    const float screenWidth = screenSizePixels.x;
    const float screenHeight = screenSizePixels.y;
    if (screenWidth <= 0.0f || screenHeight <= 0.0f) {
        // 画面サイズが無効なら、ゼロ除算を避けて現在の操舵入力を維持する。
        return;
    }

    // 照準をアナログスティックとして扱い、画面中央からのずれを入力へ変換する。
    // 中央は0、右端・上端は+1、左端・下端は-1。Yは画面座標と向きが逆になる。
    float inputX = (aimPosition.x - screenWidth * 0.5f) /(screenWidth * 0.5f);
    float inputY = (screenHeight * 0.5f - aimPosition.y) /(screenHeight * 0.5f);

    // 感度を掛けて-1～1へ制限し、中央付近の微小なずれを取り除く。
    // 照準の移動速度と機体の追従を分離する。パッドは照準のずれをそのまま操舵に使う。
    float sensitivity = mouseSensitivity_;
    if (isGamepad) { sensitivity = 1.0f; }
    inputX = std::clamp(inputX * sensitivity, -1.0f, 1.0f);
    inputY = std::clamp(inputY * sensitivity, -1.0f, 1.0f);
    // パッドのデッドゾーンはInputで適用済み。低感度でも細かな操作を残す。
    if (!isGamepad) {
        inputX = ApplySteeringDeadZone(inputX);
        inputY = ApplySteeringDeadZone(inputY);
    }

    // パッドは入力へ即時追従し、照準の移動・中央復帰にさらに遅れを重ねない。
    if (isGamepad) {
        steeringInput_ = { inputX, inputY };
        return;
    }
    // マウスのレール移動は即時追従し、全方向飛行では経過秒数から追従率を求める。
    // 全方向飛行の追従率は、フレームレートによる旋回感の差を抑えるため時間ベースにする。
    float steeringLerpRate = 1.0f;
    if (isAllRangeMode) {
        steeringLerpRate = 1.0f - std::exp(-13.4f * deltaTimeSeconds);
    }
    // 全方向飛行のマウス操舵には補間を適用する。
    steeringInput_.x += (inputX - steeringInput_.x) * steeringLerpRate;
    steeringInput_.y += (inputY - steeringInput_.y) * steeringLerpRate;
}

Vector2 PlayerMovement::CalculateScreenCorrection(const Vector3& railOffset) const
{
    Vector2 correction {};
    correction.x = 0.0f;
    correction.y = 0.0f;

    if (camera_ == nullptr) {
        return correction;
    }

    Vector3 playerPosition = CalculateRailWorldPosition(railOffset);
    Vector2 screenPosition = camera_->WorldToScreen(playerPosition);

    float minX = screenPosition.x;
    float maxX = screenPosition.x;
    float minY = screenPosition.y;
    float maxY = screenPosition.y;

    // 機体の中心だけでなく、レールの左右・上下に広がる矩形の4隅も投影する。
    Vector3 rightExtent = railRight_ * playerBoundsHalfWidth_;
    Vector3 upExtent = railUp_ * playerBoundsHalfHeight_;

    UpdateScreenBounds(playerPosition + rightExtent + upExtent, minX, maxX, minY, maxY);
    UpdateScreenBounds(playerPosition + rightExtent - upExtent, minX, maxX, minY, maxY);
    UpdateScreenBounds(playerPosition - rightExtent + upExtent, minX, maxX, minY, maxY);
    UpdateScreenBounds(playerPosition - rightExtent - upExtent, minX, maxX, minY, maxY);

    // 画面端の余白を除いた範囲を、機体を収める領域とする。
    const Vector2 clampMarginPixels = WinApp::GetInstance()->UiToClient({playerClampMarginX_, playerClampMarginY_});
    float leftLimit = clampMarginPixels.x;
    float rightLimit = static_cast<float>(WinApp::GetInstance()->GetClientWidth()) - clampMarginPixels.x;

    float topLimit = clampMarginPixels.y;
    float bottomLimit = static_cast<float>(WinApp::GetInstance()->GetClientHeight()) - clampMarginPixels.y;

    // はみ出した側を画面内へ戻す量を求める。まだワールド座標の移動量には変換しない。
    if (minX < leftLimit) {
        correction.x = leftLimit - minX;
    } else if (maxX > rightLimit) {
        correction.x = rightLimit - maxX;
    }

    if (minY < topLimit) {
        correction.y = topLimit - minY;
    } else if (maxY > bottomLimit) {
        correction.y = bottomLimit - maxY;
    }

    return correction;
}

Vector3 PlayerMovement::CalculateRailWorldPosition(const Vector3& railOffset) const
{
    // 基準位置に、レールの左右・上下・前後方向の移動量を足す。
    return railBasePosition_ + railRight_ * railOffset.x + railUp_ * railOffset.y + railForward_ * railOffset.z;
}

Vector3 PlayerMovement::ClampRailOffsetToScreen(const Vector3& railOffset) const
{
    Vector3 correctedRailOffset = railOffset;
    // 先にレール上の移動範囲へ制限する。前後方向のオフセットは使わない。
    correctedRailOffset.x = std::clamp(
        correctedRailOffset.x, -railMoveLimitX_, railMoveLimitX_);
    if (!hasRailHeightRange_) {
        correctedRailOffset.y = std::clamp(correctedRailOffset.y, -railMoveLimitY_, railMoveLimitY_);
    }
    correctedRailOffset.z = 0.0f;
    // 入力と画面内補正の両方で、その地点の高度範囲を守る。
    EnforceHeightRange(correctedRailOffset);


    if (camera_ == nullptr) {
        return correctedRailOffset;
    }

    // 投影による誤差を減らすため、補正を最大3回繰り返す。
    // カメラが未設定なら上の移動範囲の制限だけで終了する。
    constexpr int kCorrectionCount = 3;
    for (int correctionIndex = 0; correctionIndex < kCorrectionCount; ++correctionIndex) {
        Vector2 screenCorrection = CalculateScreenCorrection(correctedRailOffset);

        if (std::fabs(screenCorrection.x) < 0.01f && std::fabs(screenCorrection.y) < 0.01f) {
            break;
        }

        // 左右・上下へそれぞれ1単位動かした場合の、画面上の変化量を調べる。
        Vector2 baseScreen = camera_->WorldToScreen(CalculateRailWorldPosition(correctedRailOffset));

        Vector3 rightOffset = correctedRailOffset;
        rightOffset.x += 1.0f;
        Vector2 rightScreen = camera_->WorldToScreen(CalculateRailWorldPosition(rightOffset));

        Vector3 upOffset = correctedRailOffset;
        upOffset.y += 1.0f;
        Vector2 upScreen = camera_->WorldToScreen(CalculateRailWorldPosition(upOffset));

        float rightScreenX = rightScreen.x - baseScreen.x;
        float rightScreenY = rightScreen.y - baseScreen.y;
        float upScreenX = upScreen.x - baseScreen.x;
        float upScreenY = upScreen.y - baseScreen.y;

        // 画面上の補正量を、レールの左右・上下の移動量へ変換するための行列式。
        float determinant = rightScreenX * upScreenY - rightScreenY * upScreenX;

        if (std::fabs(determinant) > 0.0001f) {
            float offsetX = (screenCorrection.x * upScreenY - screenCorrection.y * upScreenX) / determinant;
            float offsetY = (rightScreenX * screenCorrection.y - rightScreenY * screenCorrection.x) / determinant;

            correctedRailOffset.x += offsetX;
            correctedRailOffset.y += offsetY;
        } else {
            // 投影した2方向がほぼ重なる場合は、各方向への成分を使って補正する。
            float rightLengthSquared = rightScreenX * rightScreenX + rightScreenY * rightScreenY;
            if (rightLengthSquared > 0.0001f) {
                float offsetX = (screenCorrection.x * rightScreenX + screenCorrection.y * rightScreenY) / rightLengthSquared;
                correctedRailOffset.x += offsetX;
            }

            float upLengthSquared = upScreenX * upScreenX + upScreenY * upScreenY;
            if (upLengthSquared > 0.0001f) {
                float offsetY = (screenCorrection.x * upScreenX + screenCorrection.y * upScreenY) / upLengthSquared;
                correctedRailOffset.y += offsetY;
            }
        }

        // 画面内補正でレールの移動範囲を超えないよう、もう一度制限する。
        correctedRailOffset.x = std::clamp(correctedRailOffset.x, -railMoveLimitX_, railMoveLimitX_);
        if (!hasRailHeightRange_) {
            correctedRailOffset.y = std::clamp(correctedRailOffset.y, -railMoveLimitY_, railMoveLimitY_);
        }
        correctedRailOffset.z = 0.0f;
        EnforceHeightRange(correctedRailOffset);
    }

    return correctedRailOffset;
}

void PlayerMovement::UpdateScreenBounds(const Vector3& worldPosition, float& minX, float& maxX, float& minY, float& maxY) const
{
    if (camera_ == nullptr) {
        return;
    }

    // ワールド座標を画面へ投影し、左右・上下の最小値と最大値を更新する。
    Vector2 screenPosition = camera_->WorldToScreen(worldPosition);
    minX = (std::min)(minX, screenPosition.x);
    maxX = (std::max)(maxX, screenPosition.x);
    minY = (std::min)(minY, screenPosition.y);
    maxY = (std::max)(maxY, screenPosition.y);
}

// 入力を取得した呼び出し側から、操舵の要求を受け取る。
void PlayerMovement::UpdateMovement(EulerTransform& transform, const Vector2& steering, float railResponse,
    float deltaTimeSeconds)
{
    if (isAllRangeMode_) {
        UpdateAllRangeMove(transform, steering, deltaTimeSeconds);
    } else {
        Vector3 nextRailOffset = railOffset_;
        nextRailOffset.x += steering.x * moveSpeed_ * railResponse
            * GameTiming::CalculateReferenceStep(deltaTimeSeconds);
        nextRailOffset.y += steering.y * moveSpeed_ * railResponse
            * GameTiming::CalculateReferenceStep(deltaTimeSeconds);
        railOffset_ = ClampRailOffsetToScreen(nextRailOffset);
    }
    transform.rotate.z = 0.0f;
}

// 入力後と画面内補正後に、設定された最低・最高高度を維持する。
void PlayerMovement::EnforceHeightRange(Vector3& offset) const
{
    if (hasRailHeightRange_) {
        const float baseHeight = railBasePosition_.y + railRight_.y * offset.x;
        offset.y = railHeightRange_.ClampOffset(offset.y, baseHeight, railUp_.y);
        return;
    }
    if (hasRailMinimumHeight_ && railUp_.y > 0.0001f) {
        const float minimumOffset = (railMinimumHeight_ - railBasePosition_.y - railRight_.y * offset.x) / railUp_.y;
        offset.y = (std::max)(offset.y, (std::min)(minimumOffset, railMoveLimitY_));
    }
}

// PlayerInput
PlayerInput::ControlInputType PlayerInput::GetControlInputType(const Input& input, bool shouldUseMouse)
{
    if (input.IsUsingGamepad()) {
        return ControlInputType::Gamepad;
    }
    if (shouldUseMouse) {
        return ControlInputType::Mouse;
    }
    return ControlInputType::Keyboard;
}

// デスクトップ座標を、ゲームの描画領域の左上を原点とする座標へ変換する。
Vector2 PlayerInput::ReadMousePosition()
{
    const HWND windowHandle = WinApp::GetInstance()->GetHwnd();
    POINT mousePosition = {};
    GetCursorPos(&mousePosition);
    ScreenToClient(windowHandle, &mousePosition);
    return { static_cast<float>(mousePosition.x), static_cast<float>(mousePosition.y) };
}

PlayerInput::AimInput PlayerInput::ReadGamepadAim(const Input& input)
{
    AimInput aimInput;
    aimInput.stick = { input.GetGamepadLeftStickX(), input.GetGamepadLeftStickY() };
    aimInput.isPitchInverted = input.IsGamepadPitchInverted();
    aimInput.sensitivity = input.GetGamepadAimSensitivity();
    return aimInput;
}

Vector2 PlayerInput::ReadKeyboardSteering(const Input& input)
{
    Vector2 steering = {};
    if (input.IsKeyPressed(DIK_A)) { steering.x -= 1.0f; }
    if (input.IsKeyPressed(DIK_D)) { steering.x += 1.0f; }
    if (input.IsKeyPressed(DIK_W)) { steering.y += 1.0f; }
    if (input.IsKeyPressed(DIK_S)) { steering.y -= 1.0f; }
    return steering;
}

PlayerInput::BoostInput PlayerInput::ReadBoost(const Input& input, bool isEnabled)
{
    BoostInput boostInput;
    if (!isEnabled) {
        return boostInput;
    }
    boostInput.isBraking = input.IsKeyPressed(DIK_LCONTROL) || input.IsKeyPressed(DIK_RCONTROL) ||
        input.IsGamepadButtonPressed(XINPUT_GAMEPAD_Y);
    boostInput.shouldBoost = input.IsKeyPressed(DIK_LSHIFT) || input.IsMousePressed(1) ||
        input.IsGamepadButtonPressed(XINPUT_GAMEPAD_X);
    return boostInput;
}

// PlayerEntryState
void PlayerEntryState::Update(Player& player)
{
    player.ChangeState(Player::State::Battle);
}

// PlayerBattleState
void PlayerBattleState::Update(Player& player)
{
    Input* input = Input::GetInstance();
    if (input == nullptr) {
        return;
    }
    player.UpdateInvincibility(TimeManager::GetInstance()->GetDeltaTime());
    player.UpdateBoostState();
    player.SynchronizeGamepadControlStyle(*input);
    if (player.isUsingDirectGamepadControl_ && !player.isDebugMode_) {
        // 新方式ではスティックで機体を動かし、照準と発射はカメラ確定後に行う。
        player.UpdateMovement(input);
        player.ApplyRailPosition();
        player.shouldUpdateWeaponsAfterCamera_ = true;
        return;
    }
    if (!player.isDebugMode_) {
        // 照準を先に確定してから操舵し、発射と表示で参照する位置を揃える。
        // 使用中の入力機器から照準を取得し、操舵とレティクル表示に使う。
        player.UpdateAimFromInput(*input, TimeManager::GetInstance()->GetDeltaTime());
        player.ClampAimScreenPosition();
        player.ConstrainGamepadAimToPlayer();
        player.UpdateMovement(input);
    }
    player.ApplyRailPosition();
    if (!player.isDebugMode_) {
        player.ConstrainGamepadAimToPlayer();
        if (player.camera_ != nullptr) {
            // 移動後の機体との距離を補正した照準で、発射に使う奥行きを確定する。
            player.UpdateAim(*player.camera_, TimeManager::GetInstance()->GetDeltaTime());
        }
    }
    player.ApplyTransform();
    // 最新の照準と移動後の銃口から発射し、弾はこの後で1回だけ更新する。
    player.weaponController_.UpdateInput(input, player.isDebugMode_);
    player.weaponController_.UpdateBullets();
    // 寿命切れや命中で無効になった弾を一覧から取り除く。
    player.weaponController_.RemoveDeadBullets();
    player.object_->Update();
#ifdef _DEBUG
    player.weaponController_.DrawAimDebugLines();
#endif
}

// PlayerDeadState
void PlayerDeadState::Enter(Player& player)
{
    deathTimerSeconds_ = 0.0f;
    deathFallVelocity_ = 0.0f;
    isExplosionReady_ = false;
    player.StopMovement();
    player.weaponController_.ResetHomingLock();
}

void PlayerDeadState::Update(Player& player)
{
    if (!isExplosionReady_) {
        UpdateAnimation(player.transform_, TimeManager::GetInstance()->GetDeltaTime());
        player.ApplyTransform();
    }
    // 発射済みの弾は死亡後も更新する。
    player.weaponController_.UpdateBullets();
    player.weaponController_.RemoveDeadBullets();
    player.object_->Update();
}

// 死亡時の落下・回転を更新する。落下演出が終わると死亡爆発へ進める。
void PlayerDeadState::UpdateAnimation(EulerTransform& transform, float deltaTimeSeconds)
{
    if (isExplosionReady_) {
        return;
    }

    deathTimerSeconds_ += deltaTimeSeconds;
    deathFallVelocity_ += 18.0f * deltaTimeSeconds;
    transform.translate.y -= deathFallVelocity_ * deltaTimeSeconds;
    transform.rotate.x += 1.4f * deltaTimeSeconds;
    transform.rotate.z += 3.2f * deltaTimeSeconds;

    if (deathTimerSeconds_ >= kDeathFallDurationSeconds) {
        isExplosionReady_ = true;
    }
}

// Player
// Playerの姿勢と公開APIを保持し、登場・戦闘・死亡の更新を各Stateへ委譲する。

// 機体のモデルと武器を準備し、照準と移動の初期状態を揃える。
void Player::Initialize(Model* model)
{
    assert(model != nullptr);

    object_ = std::make_unique<Object3d>();
    object_->Initialize(Object3dManager::GetInstance());
    object_->SetMaterial("resources/Shaders/Object3D/ShadowStandard");
    object_->SetNormalMap("resources/Textures/Normals/metal_detail.png", 0.25f);
    object_->SetCastShadow(true);
    object_->SetReceiveShadow(true);
    object_->SetEnableLighting(true);
    object_->SetModel(model);
    object_->SetSurfaceProperties(0.30f, 0.72f, 0.90f);

    if (camera_ != nullptr) {
        object_->SetCamera(camera_);
    }

    weaponController_.Initialize();

    transform_.scale = { 1.0f, 1.0f, 1.0f };
    transform_.rotate = { 0.0f, 0.0f, 0.0f };
    transform_.translate = { 0.0f, -2.0f, 0.0f };
    InitializeMovement();

    object_->SetScale(transform_.scale);
    object_->SetRotate(transform_.rotate);
    object_->SetTranslate(transform_.translate);

    // プレイヤー初期化時に、照準を画面中央へ置き、狙う距離を基準値に戻す。
    InitializeAim();
}

// 入力・被弾状態・移動・武器を更新し、最後に機体の姿勢を描画へ反映する。
void Player::Update()
{
    shouldUpdateWeaponsAfterCamera_ = false;
    if (object_ == nullptr || TimeManager::GetInstance()->GetDeltaTime() <= 0.0f) {
        // 未初期化・時間停止中は、入力や弾を含めて更新を進めない。
        return;
    }
    if (state_ == State::Entry) {
        entryState_.Update(*this);
    }
    switch (state_) {
    case State::Battle:
        battleState_.Update(*this);
        break;
    case State::Dead:
        deadState_.Update(*this);
        break;
    case State::Entry:
        break;
    }
}

// レールを進める前に加減速入力を読み取り、このフレームの速度を確定する。
void Player::UpdateBoostState()
{
    if (!IsAlive()) {
        StopMovement();
        return;
    }
    if (object_ == nullptr || TimeManager::GetInstance()->GetDeltaTime() <= 0.0f) {
        return;
    }
    Input* input = Input::GetInstance();
    if (input == nullptr) {
        return;
    }
    if (debugCameraController_ != nullptr) {
        isDebugMode_ = debugCameraController_->GetDebugMode();
    }
    UpdateBoost(input, isDebugMode_);
}

void Player::SetCamera(Camera* camera)
{
    camera_ = camera;
    movement_.SetCamera(camera);
    weaponController_.SetCamera(camera);
    if (object_ != nullptr) {
        object_->SetCamera(camera);
    }
}

void Player::SetDebugCameraController(DebugCameraController* debugCameraController)
{
    debugCameraController_ = debugCameraController;
}

void Player::SetEnableLighting(bool isEnabled)
{
    if (object_ != nullptr) {
        object_->SetEnableLighting(isEnabled);
    }
}

void Player::SetTranslate(const Vector3& translate)
{
    transform_.translate = translate;
    // 次のレール位置計算で、外部から設定した位置が元へ戻らないようにする。
    SynchronizePosition();
    if (object_ != nullptr) {
        ApplyTransform();
        object_->Update();
    }
}

void Player::SetRotate(const Vector3& rotate)
{
    transform_.rotate.x = rotate.x;
    transform_.rotate.y = rotate.y;
    transform_.rotate.z = rotate.z;
    SynchronizeOrientation();
}

Vector3 Player::GetAutomaticWorldVelocity() const
{
    if (IsDead()) {
        return {};
    }
    return GetAutomaticMovementVelocity();
}

// HP側の被弾判定を行い、死亡した場合は移動とロックオンも停止する。
bool Player::ApplyDamage(int damage)
{
    const bool hasAppliedDamage = ApplyHealthDamage(damage);
    if (hasAppliedDamage && IsDead()) {
        // 死亡状態への移行に合わせ、ブースト・ロックオンを解除する。
        ChangeState(State::Dead);
    }
    return hasAppliedDamage;
}

// 現在表示するモデルや追加パーツを描画する。
void Player::Draw()
{
    if (object_ == nullptr) {
        return;
    }
    weaponController_.DrawBullets();
    // 被弾後の点滅で機体を隠す場合も、発射済みの弾は表示する。
    if (ShouldDraw()) {
        object_->Draw();
    }
}

// 影を落とす対象のモデルを、影用の描画処理へ渡す。
void Player::DrawShadow(ShadowMapRenderer& renderer)
{
    if (object_ != nullptr && ShouldDraw()) {
        object_->DrawShadow(renderer);
    }
}

// 描画対象をレイトレーシング用のシーンへ登録する。
void Player::SubmitRaytracing(DxrRenderer& renderer) {
    if (!object_) { return; }
    weaponController_.SubmitRaytracing(renderer);
    if (ShouldDraw()) { object_->SubmitRaytracing(renderer); }
}

// 機体の位置・回転・拡大率を描画オブジェクトへ反映する。
void Player::ApplyTransform()
{
    object_->SetScale(transform_.scale);
    object_->SetRotate(transform_.rotate);
    object_->SetTranslate(transform_.translate);
}

// 開発用の画面に内部状態や調整項目を表示する。
void Player::DrawImGui()
{
#ifdef _DEBUG
    ImGui::Begin("Player Controls");
    ImGui::Text("HP: %d / %d", GetCurrentHp(), GetMaxHp());
    ImGui::Text("Weapon: %s", weaponController_.GetCurrentWeaponName());
    ImGui::Text("Heat: %.2f", weaponController_.GetHeatRatio());
    bool isAimDistanceSmoothingEnabled = aimController_.IsAimDistanceSmoothingEnabled();
    if (ImGui::Checkbox("Aim Distance Smoothing", &isAimDistanceSmoothingEnabled)) {
        aimController_.SetAimDistanceSmoothingEnabled(isAimDistanceSmoothingEnabled);
    }
    ImGui::Text("Aim distance: %.2f", aimController_.GetAimDistance());
    ImGui::End();
#endif
}
Vector3 Player::GetEngineExhaustPosition() const
{
    const Matrix4x4 worldMatrix = MatrixMath::MakeAffineMatrix(
        transform_.scale, transform_.rotate, transform_.translate);
    return MatrixMath::Transform(Vector3 { 0.0f, 0.0f, -1.0f }, worldMatrix);
}

Vector3 Player::GetEngineExhaustDirection() const
{
    const Matrix4x4 rotationMatrix = MatrixMath::MakeAffineMatrix(
        Vector3 { 1.0f, 1.0f, 1.0f }, transform_.rotate, Vector3 {});
    return Normalize(MatrixMath::Transform(Vector3 { 0.0f, 0.0f, -1.0f }, rotationMatrix));
}

// 使用中のパッドを優先し、パッドでなければ設定された操作方式を選ぶ。
Player::ControlInputType Player::GetControlInputType(const Input& input) const
{
    return PlayerInput::GetControlInputType(input, controlMode_ == ControlMode::Controller);
}

// 選択された操作方式に合わせて、マウスまたはパッドの照準を更新する。
void Player::UpdateAimFromInput(const Input& input, float deltaTimeSeconds)
{
    if (GetControlInputType(input) == ControlInputType::Gamepad) {
        const PlayerInput::AimInput aimInput = PlayerInput::ReadGamepadAim(input);
        UpdateGamepadAim(aimInput.stick.x, aimInput.stick.y,
            aimInput.isPitchInverted, deltaTimeSeconds, aimInput.sensitivity);
    } else {
        UpdateMouseAim();
    }
}

// 照準の画面位置を機体の左右・上下の操舵入力へ変換する。
Player::ControlInput Player::UpdateAimSteering(const Vector2& screenSizePixels,
    bool isAllRangeMode, float deltaTimeSeconds, bool isGamepad)
{
    Vector2 aimPosition = GetAimScreenPosition();
    if (isGamepad) { aimPosition = GetGamepadSteeringScreenPosition(); }
    UpdateSteering(aimPosition, screenSizePixels, isAllRangeMode, deltaTimeSeconds, isGamepad);
    return { movement_.GetSteeringInput(), 1.5f };
}

// キーボード・マウス・パッドの違いを、移動処理が共通に扱える入力へまとめる。
Player::ControlInput Player::GetControlInput(const Input& input, const Vector2& screenSizePixels,
    bool isAllRangeMode, float deltaTimeSeconds)
{
    const ControlInputType inputType = GetControlInputType(input);
    if (inputType == ControlInputType::Gamepad) {
        if (input.GetGamepadControlStyle() == Input::GamepadControlStyle::ExZodiac) {
            return GetDirectGamepadControl(input, deltaTimeSeconds);
        }
        return UpdateAimSteering(screenSizePixels, isAllRangeMode, deltaTimeSeconds, true);
    }
    if (inputType == ControlInputType::Mouse) {
        return UpdateAimSteering(screenSizePixels, isAllRangeMode, deltaTimeSeconds, false);
    }
    if (isAllRangeMode) { UpdateAimSteering(screenSizePixels, true, deltaTimeSeconds, false); }
    return { PlayerInput::ReadKeyboardSteering(input), 1.0f };
}

void Player::SynchronizeGamepadControlStyle(const Input& input)
{
    const bool shouldUseDirectControl = GetControlInputType(input) == ControlInputType::Gamepad &&
        input.GetGamepadControlStyle() == Input::GamepadControlStyle::ExZodiac;
    if (isUsingDirectGamepadControl_ == shouldUseDirectControl) { return; }
    isUsingDirectGamepadControl_ = shouldUseDirectControl;
    directGamepadAimInput_ = {};
    ResetSteering();
    InitializeAim();
    BlockFireUntilRelease();
}

Player::ControlInput Player::GetDirectGamepadControl(const Input& input, float deltaTimeSeconds)
{
    Vector2 steering { input.GetGamepadLeftStickX(), input.GetGamepadLeftStickY() };
    if (input.IsGamepadPitchInverted()) { steering.y = -steering.y; }
    const float magnitude = std::sqrt(steering.x * steering.x + steering.y * steering.y);
    const float response = 0.5f + 0.5f * (std::min)(magnitude, 1.0f);
    steering.x *= response;
    steering.y *= response;
    movement_.SetSteeringInput(steering);
    constexpr float kDirectAimFollowSpeed = 4.5f;
    constexpr float kDirectMovementResponse = 1.5f;
    const float followRate = 1.0f - std::exp(-kDirectAimFollowSpeed * (std::max)(0.0f, deltaTimeSeconds));
    const float sensitivity = input.GetGamepadAimSensitivity();
    const Vector2 targetAimInput {
        std::clamp(steering.x * sensitivity, -1.0f, 1.0f),
        std::clamp(steering.y * sensitivity, -1.0f, 1.0f)
    };
    directGamepadAimInput_.x += (targetAimInput.x - directGamepadAimInput_.x) * followRate;
    directGamepadAimInput_.y += (targetAimInput.y - directGamepadAimInput_.y) * followRate;
    return { steering, kDirectMovementResponse };
}

// 銃口から遠方の照準へ向かう射線上に、近距離表示用の点を置く。
Vector3 Player::GetNearAimPoint(const Camera& activeCamera) const
{
    constexpr float kNearAimDistance = 45.0f;
    constexpr float kMinAimDistance = 0.0001f;
    const Vector3 muzzlePosition = weaponController_.CalculateMuzzlePosition();
    const Vector3 aimOffset = GetAimPoint(activeCamera) - muzzlePosition;
    const float aimDistance = Vector3Length(aimOffset);
    if (aimDistance <= kMinAimDistance) { return muzzlePosition; }
    const float nearDistance = (std::min)(kNearAimDistance, aimDistance * 0.5f);
    return muzzlePosition + aimOffset * (nearDistance / aimDistance);
}

Vector3 Player::GetDirectGamepadAimDirection() const
{
    if (movement_.IsAllRangeMode()) { return movement_.GetFlightForward(); }
    const float yawRadians = directGamepadAimInput_.x * kDirectAimYawRadians;
    const float pitchRadians = directGamepadAimInput_.y * kDirectAimPitchRadians;
    return Normalize(movement_.GetFlightForward() +
        movement_.GetRailRight() * std::tan(yawRadians) +
        movement_.GetRailUp() * std::tan(pitchRadians));
}

// 描画用カメラが確定した後、機首から照準を投影して発射する。
void Player::UpdateAfterCamera(const Camera& activeCamera, float deltaTimeSeconds)
{
    if (!shouldUpdateWeaponsAfterCamera_ || state_ != State::Battle || deltaTimeSeconds <= 0.0f) { return; }
    shouldUpdateWeaponsAfterCamera_ = false;
    Input* input = Input::GetInstance();
    if (input == nullptr) { return; }
    aimController_.UpdateFromShip(activeCamera, transform_.translate,
        GetDirectGamepadAimDirection(), deltaTimeSeconds);
    ApplyTransform();
    object_->Update();
    weaponController_.UpdateInput(input, false);
    weaponController_.UpdateBullets();
    weaponController_.RemoveDeadBullets();
#ifdef _DEBUG
    weaponController_.DrawAimDebugLines();
#endif
}

// 操作できる状態かを確認してから、機体の画面位置を基準にパッド照準を補正する。
void Player::ConstrainGamepadAimToPlayer()
{
    if (isUsingDirectGamepadControl_) { return; }
    if (isDebugMode_ || !IsAlive()) { return; }
    const Input* input = Input::GetInstance();
    if (input == nullptr || GetControlInputType(*input) != ControlInputType::Gamepad || camera_ == nullptr) { return; }
    const Vector3 viewPosition = MatrixMath::Transform(transform_.translate, camera_->GetViewMatrix());
    if (!std::isfinite(viewPosition.z) || viewPosition.z <= camera_->GetNearClip()) { return; }
    ClampGamepadAimToPlayer(camera_->WorldToScreen(transform_.translate));
}

// 被弾後の無敵時間を経過秒数で進める。残り時間が0になると通常状態に戻る。
void Player::UpdateInvincibility(float deltaTimeSeconds)
{
    if (invincibleRemainingSeconds_ > 0.000001f) {
        invincibleRemainingSeconds_ = (std::max)(0.0f, invincibleRemainingSeconds_ - deltaTimeSeconds);
    }
}

// 無敵中の点滅と死亡状態に応じて描画を判定する。
bool Player::ShouldDraw() const
{
    if (state_ == State::Dead) {
        return deadState_.ShouldDraw();
    }
    return invincibleRemainingSeconds_ <= 0.000001f || static_cast<int>(invincibleRemainingSeconds_ * 15.0f + 0.00001f) % 2 == 0;
}

// 無敵時間を確認し、HPを減らす。死亡状態への遷移は呼び出し元で行う。
bool Player::ApplyHealthDamage(int damage)
{
#if defined(ENABLE_DEVELOPMENT_TOOLS)
    if (isInvincibleMode_) {
        return false;
    }
#endif
    if (damage <= 0 || invincibleRemainingSeconds_ > 0.000001f || currentHp_ <= 0) {
        return false;
    }

    currentHp_ -= damage;
    SoundManager::GetInstance()->Play("PlayerDamage");
    if (currentHp_ < 0) {
        currentHp_ = 0;
    }
    invincibleRemainingSeconds_ = kInvincibleDurationSeconds;
    return true;
}
// 生存中のHPを上限まで回復する。回復量が0以下、死亡中、またはHPが上限に達している場合は回復しない。
bool Player::Heal(int amount)
{
    if (amount <= 0 || currentHp_ <= 0 || currentHp_ >= maxHp_) {
        return false;
    }

    currentHp_ += amount;
    SoundManager::GetInstance()->Play("HealPickup");
    if (currentHp_ > maxHp_) {
        currentHp_ = maxHp_;
    }

    return true;
}
// 初期位置と姿勢を移動用の内部状態へ揃える。
void Player::InitializeMovement()
{
    movement_.InitializeMovement(transform_);
}

// 外部から設定された機体位置を、レール基準の移動状態へ反映する。
void Player::SynchronizePosition()
{
    movement_.SynchronizePosition(transform_);
}

// 外部から設定された回転角を、全方向飛行の機首方向へ反映する。
void Player::SynchronizeOrientation()
{
    movement_.SynchronizeOrientation(transform_);
}

// 操作入力を読み、レール飛行か全方向飛行かに応じて移動処理を選ぶ。
void Player::UpdateMovement(Input* input)
{
    const float deltaTimeSeconds = TimeManager::GetInstance()->GetDeltaTime();
    const ControlInput control = GetControlInput(*input,
        { static_cast<float>(WinApp::GetInstance()->GetClientWidth()),
          static_cast<float>(WinApp::GetInstance()->GetClientHeight()) },
        movement_.IsAllRangeMode(), deltaTimeSeconds);
    movement_.UpdateMovement(transform_, control.steering, control.railResponse, deltaTimeSeconds);
}

// レール基準位置と操縦によるずれを合成し、画面内に収まる機体位置を決める。
void Player::ApplyRailPosition()
{
    movement_.ApplyRailPosition(transform_);
}

// 死亡や操作停止時に、速度とロールなどの移動状態を解除する。
void Player::StopMovement()
{
    movement_.StopMovement();
}

Vector3 Player::GetAutomaticMovementVelocity() const
{
    return movement_.GetAutomaticMovementVelocity();
}

void Player::SetRailFrame(const Vector3& railBasePosition,
    const Vector3& railRight, const Vector3& railUp, const Vector3& railForward)
{
    movement_.SetRailFrame(railBasePosition, railRight, railUp, railForward);
}

void Player::ApplyRailAreaForce(const Vector3& force)
{
    movement_.ApplyRailAreaForce(force, TimeManager::GetInstance()->GetDeltaTime());
}

void Player::SetControlMode(ControlMode mode)
{
    if (controlMode_ != mode) {
        // 操作方式を変えた直後に、前のマウス操舵が残らないようにする。
        ResetSteering();
    }
    controlMode_ = mode;
}
// 加速・減速の入力を確認し、目標速度へ滑らかに近づける。
void Player::UpdateBoost(Input* input, bool isDebugMode)
{
    const PlayerInput::BoostInput boostInput = PlayerInput::ReadBoost(*input, !isDebugMode);
    if (movement_.UpdateBoost(boostInput.isBraking, boostInput.shouldBoost)) {
        SoundManager::GetInstance()->Play("BoostStart");
    }
}

void Player::EnableAllRangeMode(float areaRadius, float minHeight, float maxHeight)
{
    movement_.EnableAllRangeMode(transform_, areaRadius, minHeight, maxHeight);
}

void Player::SetMouseSensitivity(float sensitivity)
{
    movement_.SetMouseSensitivity(sensitivity);
}

// 照準の中心からのずれにデッドゾーンと感度を反映し、操舵入力を滑らかにする。
void Player::UpdateSteering(const Vector2& aimPosition, const Vector2& screenSizePixels,
    bool isAllRangeMode, float deltaTimeSeconds, bool isGamepad)
{
    movement_.UpdateSteering(aimPosition, screenSizePixels, isAllRangeMode, deltaTimeSeconds, isGamepad);
}

// レールの基準位置に左右・上下のずれを加え、ワールド位置へ変換する。
Vector3 Player::CalculateRailWorldPosition(const Vector3& railOffset) const
{
    return movement_.CalculateRailWorldPosition(railOffset);
}

// 機体の大きさとカメラ投影を考慮し、画面からはみ出ないレール上のずれへ補正する。
Vector3 Player::ClampRailOffsetToScreen(const Vector3& railOffset) const
{
    return movement_.ClampRailOffsetToScreen(railOffset);
}

// 状態の切替と開始処理を一か所で行う。
void Player::ChangeState(State state)
{
    if (state_ == state) {
        return;
    }
    state_ = state;
    if (state_ == State::Dead) {
        deadState_.Enter(*this);
    }
}

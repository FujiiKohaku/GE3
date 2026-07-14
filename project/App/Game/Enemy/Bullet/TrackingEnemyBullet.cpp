#include "App/Game/Enemy/Bullet/TrackingEnemyBullet.h"
#include "App/Game/Player/Player.h"
#include "Engine/math/MathStruct.h"
#include <cmath>

namespace {
constexpr float kLaunchDuration = 0.75f;     // 打ち上げフェーズの時間（秒）
constexpr float kLaunchUpSpeed = 1.35f;      // 打ち上げ時の初期上方向速度（減速させていく）
constexpr float kMissileStartSpeed = 0.15f;  // 追尾開始時の初速
constexpr float kMissileMaxSpeed = 1.35f;    // 追尾時の最高速度
constexpr float kAccelDuration = 1.50f;      // 最高速度に達するまでの加速時間（秒）
constexpr float kTrackingRate = 0.050f;      // 追尾旋回率
constexpr float kTrackingDuration = 1.50f;   // 追尾する最大時間（秒）
}

void TrackingEnemyBullet::Initialize(Model* model)
{
    transform_.scale = { 0.45f, 0.45f, 0.45f }; // ミサイルらしく少し目立たせる
    maxLifeTime_ = 6.5f;                        // 追尾時間を与えるために長めに設定
    collisionRadius_ = 1.8f;
    damage_ = 2;                                // 追尾ミサイルなので通常弾より少し高ダメージ

    EnemyBullet::Initialize(model);
}

void TrackingEnemyBullet::Update()
{
    if (!isAlive_) {
        return;
    }

    // 移動および寿命処理（内部でMoveが呼ばれる）
    EnemyBullet::Update();

    // 進行方向を向くように回転を設定
    if (Vector3Length(velocity_) > 0.001f) {
        Vector3 dir = NormalizeSafe(velocity_);
        transform_.rotate.x = -std::asin(dir.y);
        transform_.rotate.y = std::atan2(dir.x, dir.z);
    }
}

void TrackingEnemyBullet::Move()
{
    if (launchTimer_ < kLaunchDuration) {
        launchTimer_ += 1.0f / 60.0f;
        // 1. 打ち上げフェーズ（EaseOutの減速上昇）
        float t = launchTimer_ / kLaunchDuration;
        float easeOut = 1.0f - (t * t); // イージング（後半ほど減速）
        velocity_ = { 0.0f, kLaunchUpSpeed * easeOut, 0.0f };
    } else {
        // 2. 追尾・加速フェーズ
        if (!isTracking_ && !isPassed_) {
            isTracking_ = true;
        }

        if (player_ != nullptr) {
            Vector3 toPlayer = player_->GetTranslate() - transform_.translate;

            // プレイヤーを通り過ぎたか、あるいは追尾制限時間を過ぎた場合に追尾終了
            if (isTracking_ && !isPassed_) {
                trackingTimer_ += 1.0f / 60.0f;

                float dot = Dot(NormalizeSafe(toPlayer), NormalizeSafe(velocity_));
                // すれ違ったか、あるいは最大追尾時間を経過したら外れたとみなす
                if (dot < 0.0f || trackingTimer_ >= kTrackingDuration) {
                    isPassed_ = true;
                    isTracking_ = false;
                }
            }

            if (isTracking_) {
                // 加速イージング (EaseInQuad)
                float accelT = std::min(1.0f, trackingTimer_ / kAccelDuration);
                float easeIn = accelT * accelT;
                float currentSpeed = kMissileStartSpeed + (kMissileMaxSpeed - kMissileStartSpeed) * easeIn;

                Vector3 targetDirection = NormalizeSafe(toPlayer);
                Vector3 targetVelocity = targetDirection * currentSpeed;

                velocity_ = Lerp(velocity_, targetVelocity, kTrackingRate);
            }
        }
    }

    // 座標更新
    transform_.translate += velocity_;
}

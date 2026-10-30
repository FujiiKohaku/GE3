// 群れの敵が撃つ弾。進行方向に対して横へ揺れる軌道を計算する。
#include "App/Game/Common/GameTiming.h"
#include "Engine/Time/TimeManager.h"
#include "App/Game/Enemy/Bullet/Types/SwarmPulseBullet.h"

#include "Engine/math/MathStruct.h"
#include "Engine/Time/TimeManager.h"
#include <cmath>

// 群れ用の弾の色や材質と、波状移動の初期状態を準備する。
void SwarmPulseBullet::Initialize(Model* model)
{
    transform_.scale = { 0.22f, 0.22f, 0.22f };
    // 波状軌道の途中で時間切れにならないよう余裕を持たせる。
    SetMaxLifeTime(20.0f);
    SetCollisionRadius(1.0f);
    SetDamage(1);

    EnemyBullet::Initialize(model);
    SetEnableLighting(false);
    SetPixelShaderPath("resources/Shaders/Object3D/DanmakuBullet/Render.PS.hlsl");
    SetTransparent(true);
    SetTransparentDepthWrite(false);
    SetCastShadow(false);
    SetColor({ 1.0f, 0.0f, 0.0f, 1.0f });
}

// 発射位置を設定し、波状軌道の基準位置も揃える。
void SwarmPulseBullet::SetTranslate(const Vector3& translate)
{
    pathPosition_ = translate;
    EnemyBullet::SetTranslate(translate);
}

// 進行速度から、弾を揺らすための横方向などを求める。
void SwarmPulseBullet::SetSwarmVelocity(const Vector3& velocity)
{
    velocity_ = velocity;

    Vector3 forward = Normalize(velocity_);
    Vector3 referenceUp = { 0.0f, 1.0f, 0.0f };
    waveSide_ = Normalize(Cross(referenceUp, forward));
    if (IsNearlyZero(waveSide_)) {
        waveSide_ = { 1.0f, 0.0f, 0.0f };
    }

    waveUp_ = Normalize(Cross(forward, waveSide_));
    if (IsNearlyZero(waveUp_)) {
        waveUp_ = referenceUp;
    }
}

// 弾ごとの揺れの開始角度を設定し、弾幕にずれを付ける。
void SwarmPulseBullet::SetWavePhase(float phase)
{
    wavePhase_ = phase;
}

// 直進する基準位置に横方向の揺れを加えて、弾の現在位置を求める。
void SwarmPulseBullet::Move()
{
    const float timeScale = GetTimeScale();
    const float deltaTimeSeconds = TimeManager::GetInstance()->GetDeltaTime() * timeScale;
    if (deltaTimeSeconds <= 0.0f) {
        return;
    }
    const float referenceStep = GameTiming::CalculateReferenceStep(deltaTimeSeconds);
    waveTime_ += deltaTimeSeconds;

    pathPosition_.x += velocity_.x * referenceStep;
    pathPosition_.y += velocity_.y * referenceStep;
    pathPosition_.z += velocity_.z * referenceStep;

    float waveAngle = waveTime_ * waveFrequency_ + wavePhase_;
    float sideOffset = std::sin(waveAngle) * waveAmplitude_;
    float upOffset = std::cos(waveAngle) * waveAmplitude_ * 0.55f;

    transform_.translate = pathPosition_;
    transform_.translate += waveSide_ * sideOffset;
    transform_.translate += waveUp_ * upOffset;
    transform_.rotate.z += 13.2f * deltaTimeSeconds;
}

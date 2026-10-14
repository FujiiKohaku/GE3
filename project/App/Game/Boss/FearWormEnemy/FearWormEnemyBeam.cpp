#include "FearWormEnemy.h"
#include "App/Game/Player/Player.h"
#include "Engine/3D/Object3d.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/3D/ModelManager.h"
#include "Engine/Effect/EffectManager.h"
#include "Engine/Time/TimeManager.h"
#include <algorithm>
#include <cmath>

void FearWormEnemy::InitializeBeam()
{
    // ビーム用クロスPlaneモデルのロード
    Model* beamModel = ModelManager::GetInstance()->CreateBeamCross("resources/Textures/beam.jpg");

    // ビームオブジェクトの初期化
    beamPlane_ = std::make_unique<Object3d>();
    beamPlane_->Initialize(Object3dManager::GetInstance());
    beamPlane_->SetModel(beamModel);
    beamPlane_->SetEnableLighting(false);

    // 初期状態の設定
    beamState_ = BeamState::Wait;
    beamTimer_ = 0.0f;
    beamUVScrollOffset_ = 0.0f;
    beamCurrentLength_ = 0.0f;
    beamCurrentWidth_ = 0.0f;
    beamRotateTheta_ = 0.0f;
}

void FearWormEnemy::UpdateBeamAttack()
{
    if (player_ == nullptr) {
        return;
    }

    float deltaTime = TimeManager::GetInstance()->GetDeltaTime();
    beamTimer_ += deltaTime;

    // UVスクロールオフセットを更新
    constexpr float kUVScrollSpeed = -6.5f; // より高速に流す
    beamUVScrollOffset_ += kUVScrollSpeed * deltaTime;
    if (beamUVScrollOffset_ < -1.0f) {
        beamUVScrollOffset_ += 1.0f;
    }

    // UVスクロール行列を作成してマテリアルにセット
    Vector3 uvTranslation = { 0.0f, beamUVScrollOffset_, 0.0f }; // V方向にスクロール
    Matrix4x4 uvMat = MatrixMath::MakeTranslateMatrix(uvTranslation);
    if (beamPlane_->GetMaterial()) {
        beamPlane_->SetUvTransform(uvMat);
    }

    // ビームの自転角度を更新
    constexpr float kRotateSpeed = 45.0f; // 高速自転（うねりの表現を強化）
    beamRotateTheta_ += kRotateSpeed * deltaTime;
    constexpr float kPi = 3.1415926535f;
    if (beamRotateTheta_ > kPi * 2.0f) {
        beamRotateTheta_ -= kPi * 2.0f;
    }

    // プレイヤーの座標
    Vector3 headPos = HeadMuzzlePosition();

    // 照準を更新する前のマズル位置を、予兆・発射エフェクトで共有する。
    beamMuzzlePosition_ = headPos;
    UpdateBeamState(deltaTime);

    // 1. 射向ベクトル (Lerpでゆっくり追従しているheadAimDirection_をそのままビームの進行方向軸とする)
    Vector3 zAxis = headAimDirection_;
    float length = beamCurrentLength_;
    if (length < 0.001f) length = 0.001f;

    // 2. 正規直交基底の作成 (仮の上方向と射向ベクトルの外積から右方向を算出)
    Vector3 tempUp = { 0.0f, 1.0f, 0.0f };
    if (std::abs(zAxis.y) > 0.999f) {
        tempUp = { 0.0f, 0.0f, 1.0f }; // 真上・真下を向いている場合のフォールバック
    }
    
    Vector3 xAxis = NormalizeSafe(Cross(tempUp, zAxis));
    Vector3 yAxis = Cross(zAxis, xAxis); // すでに直交かつ正規化されている

    // 3. 進行方向(zAxis)まわりの自転適用 (xAxis と yAxis を theta で回転)
    float cosT = std::cos(beamRotateTheta_);
    float sinT = std::sin(beamRotateTheta_);

    Vector3 xAxisRot = {
        xAxis.x * cosT + yAxis.x * sinT,
        xAxis.y * cosT + yAxis.y * sinT,
        xAxis.z * cosT + yAxis.z * sinT
    };

    Vector3 yAxisRot = {
        -xAxis.x * sinT + yAxis.x * cosT,
        -xAxis.y * sinT + yAxis.y * cosT,
        -xAxis.z * sinT + yAxis.z * cosT
    };

    // 4. 回転行列の直接構築 (基底ベクトルを直接代入)
    Matrix4x4 matRot = MatrixMath::MakeIdentity4x4();
    matRot.m[0][0] = xAxisRot.x;  matRot.m[0][1] = xAxisRot.y;  matRot.m[0][2] = xAxisRot.z;
    matRot.m[1][0] = yAxisRot.x;  matRot.m[1][1] = yAxisRot.y;  matRot.m[1][2] = yAxisRot.z;
    matRot.m[2][0] = zAxis.x;     matRot.m[2][1] = zAxis.y;     matRot.m[2][2] = zAxis.z;

    // 5. スケールと平行移動の合成 (Scale -> Rotate -> Translate)
    Matrix4x4 matScale = MatrixMath::Matrix4x4MakeScaleMatrix({ beamCurrentWidth_, beamCurrentWidth_, length });
    Matrix4x4 matTrans = MatrixMath::MakeTranslateMatrix(headPos);

    Matrix4x4 worldMat = MatrixMath::Multiply(MatrixMath::Multiply(matScale, matRot), matTrans);

    // 6. ワールド行列を直接設定
    beamPlane_->SetCustomWorldMatrix(worldMat);
    beamPlane_->Update();
}

void FearWormEnemy::DrawBeam()
{
    if (GetBossState() == BossState::Wait || segments_.empty()) {
        return;
    }

    if (!segments_[0].isAlive) {
        return;
    }

    if (beamState_ == BeamState::Charge || beamState_ == BeamState::Fire || beamState_ == BeamState::FadeOut) {
        if (beamPlane_) {
            // 加算ブレンドを適用して、黒背景を透過させ発光感を出す
            Object3dManager::GetInstance()->SetBlendMode(kBlendModeAdd);
            beamPlane_->Draw();
            // 通常ブレンドに戻す
            Object3dManager::GetInstance()->SetBlendMode(kBlendModeNormal);
        }
    }
}

void FearWormEnemy::CheckBeamCollision()
{
    isBeamHittingPlayer_ = false; // 毎フレーム初期化

    if (player_ == nullptr || beamCurrentLength_ <= 0.0f || beamCurrentWidth_ <= 0.0f) {
        return;
    }

    // ビームの始点と終点
    Vector3 beamStart = HeadMuzzlePosition();
    Vector3 beamEnd = beamStart + headAimDirection_ * beamCurrentLength_;

    // プレイヤーの座標と半径
    Vector3 playerPos = player_->GetTranslate();
    float playerRadius = 1.0f;
    float beamRadius = beamCurrentWidth_ * 0.5f;

    // 線分 AB (ビーム) と点 C (プレイヤー) の最短距離を求める
    Vector3 AB = beamEnd - beamStart;
    Vector3 AC = playerPos - beamStart;

    float abLenSq = Vector3LengthSquared(AB);
    float t = 0.0f;
    if (abLenSq > 0.0f) {
        t = Dot(AC, AB) / abLenSq;
        t = std::clamp(t, 0.0f, 1.0f);
    }

    // 最短点 P
    Vector3 P = beamStart + AB * t;

    // 最短距離の2乗
    Vector3 diff = playerPos - P;
    float distanceSq = Vector3LengthSquared(diff);

    // 衝突半径の合計の2乗
    float hitRadius = beamRadius + playerRadius;
    float hitRadiusSq = hitRadius * hitRadius;

    if (distanceSq <= hitRadiusSq) {
        isBeamHittingPlayer_ = true; // プレイヤーへの接触状態をON

        constexpr int kBeamDamage = 1;
        player_->ApplyDamage(kBeamDamage); // HP減少は無敵時間が適用される

        // 無敵時間に関わらず、接触している間は被弾火花エフェクトを3フレームに1回に間引いて発生させる (FPS低下防止)
        beamHitEffectFrameCount_++;
        if (beamHitEffectFrameCount_ >= 3) {
            beamHitEffectFrameCount_ = 0;
            EffectManager::GetInstance()->PlayEffect("DamageHit", playerPos);
        }
    } else {
        beamHitEffectFrameCount_ = 0;
    }
}

void FearWormEnemy::UpdateBeamState(float deltaTime)
{
    switch (beamState_) {
    case BeamState::Wait: UpdateBeamWait(deltaTime); break;
    case BeamState::Charge: UpdateBeamCharge(deltaTime); break;
    case BeamState::Fire: UpdateBeamFire(deltaTime); break;
    case BeamState::FadeOut: UpdateBeamFadeOut(deltaTime); break;
    }
}

void FearWormEnemy::UpdateBeamWait(float)
{
    beamCurrentLength_ = 0.0f;
    beamCurrentWidth_ = 0.0f;
    Attack();
    if (beamTimer_ >= 4.0f) ChangeBeamState(BeamState::Charge);
}

void FearWormEnemy::UpdateBeamCharge(float dt)
{
    beamCurrentLength_ = 0.0f;
    beamCurrentWidth_ = 0.0f;
    const Vector3 direction = NormalizeSafe(player_->GetTranslate() - segments_[0].position);
    headAimDirection_ = NormalizeSafe(Lerp(headAimDirection_, direction, 0.08f));
    beamEffectTimer_ += dt;
    if (beamEffectTimer_ >= 0.25f) {
        beamEffectTimer_ = 0.0f;
        EffectManager::GetInstance()->PlayEffect("WormShotFlash", beamMuzzlePosition_);
    }
    if (beamTimer_ >= 1.5f) ChangeBeamState(BeamState::Fire);
}

void FearWormEnemy::UpdateBeamFire(float dt)
{
    beamCurrentLength_ = (std::min)(400.0f, beamCurrentLength_ + 1000.0f * dt);
    beamCurrentWidth_ = 3.5f;
    const Vector3 direction = NormalizeSafe(player_->GetTranslate() - segments_[0].position);
    headAimDirection_ = NormalizeSafe(Lerp(headAimDirection_, direction, 0.015f));
    CheckBeamCollision();
    beamEffectTimer_ += dt;
    if (beamEffectTimer_ >= 0.2f) {
        beamEffectTimer_ = 0.0f;
        EffectManager::GetInstance()->PlayEffect("WormShotFlash", beamMuzzlePosition_);
    }
    if (beamTimer_ >= 3.0f) ChangeBeamState(BeamState::FadeOut);
}

void FearWormEnemy::UpdateBeamFadeOut(float dt)
{
    beamCurrentWidth_ = (std::max)(0.0f, beamCurrentWidth_ - 8.0f * dt);
    const Vector3 direction = NormalizeSafe(player_->GetTranslate() - segments_[0].position);
    headAimDirection_ = NormalizeSafe(Lerp(headAimDirection_, direction, 0.01f));
    if (beamTimer_ >= 0.5f) ChangeBeamState(BeamState::Wait);
}

void FearWormEnemy::ChangeBeamState(BeamState state)
{
    if (beamState_ == state) return;
    beamState_ = state;
    beamTimer_ = 0.0f;
    switch (state) {
    case BeamState::Charge: {
        if (isHeadChargeActive_) FinishHeadChargeAttack();
        beamEffectTimer_ = 0.0f;
        if (player_ && !segments_.empty()) {
            headAimDirection_ = NormalizeSafe(player_->GetTranslate() - segments_[0].position);
            EffectManager::GetInstance()->PlayEffect("WormShotFlash", beamMuzzlePosition_);
        }
        break;
    }
    case BeamState::Fire: {
        beamEffectTimer_ = 0.0f;
        if (player_ && !segments_.empty()) {
            EffectManager::GetInstance()->PlayEffect("WormShotFlash", beamMuzzlePosition_);
        }
        break;
    }
    default: break;
    }
}

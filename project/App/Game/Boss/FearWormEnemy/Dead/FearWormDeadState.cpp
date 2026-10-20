#include "FearWormDeadState.h"
#include "../FearWormEnemy.h"
#include "Engine/Time/TimeManager.h"
#include "Engine/Effect/EffectManager.h"
#include "Engine/3D/Object3d.h"

void FearWormDeadState::Enter(FearWormEnemy& boss)
{
    // 攻撃状態をクリア（発射済みの弾はManager側で飛び続ける）
    boss.isHeadChargeActive_ = false;
    boss.isHeadChargeFiring_ = false;
    boss.ChangeBeamState(FearWormEnemy::BeamState::Wait);
    boss.beamCurrentLength_ = 0.0f;
    boss.beamCurrentWidth_ = 0.0f;
    isDeathSequenceFinished_ = false;
    elapsedSeconds_ = 0.0f;

    // 胴体セグメントは即座にすべて非表示にする
    for (size_t index = 1; index < boss.segments_.size(); ++index) {
        boss.segments_[index].isAlive = false;
    }

    // 頭部の位置で大爆発エフェクトを即座に再生
    EffectManager::GetInstance()->PlayEffect("WormDeathExplosion", boss.segments_[0].position);

    // 落下物理パラメータの初期化
    // 前方（Zマイナス方向＝手前）へ飛び出しながら、上方向にポンと跳ね上がる初速を設定
    velocityUnitsPerSecond_ = kInitialVelocityUnitsPerSecond;
    rotationRadians_ = boss.segments_[0].object->GetRotate();
}

void FearWormDeadState::Update(FearWormEnemy& boss)
{
    if (isDeathSequenceFinished_) {
        return;
    }

    const float deltaTimeSeconds = TimeManager::GetInstance()->GetDeltaTime();
    elapsedSeconds_ += deltaTimeSeconds;

    // 重力を適用して落下速度を下方向（Yマイナス）に引っ張る
    velocityUnitsPerSecond_.y -= kGravityUnitsPerSecondSquared * deltaTimeSeconds;

    // 速度を頭部座標に適用
    boss.segments_[0].position.x += velocityUnitsPerSecond_.x * deltaTimeSeconds;
    boss.segments_[0].position.y += velocityUnitsPerSecond_.y * deltaTimeSeconds;
    boss.segments_[0].position.z += velocityUnitsPerSecond_.z * deltaTimeSeconds;

    // 回転速度を頭部回転角に適用
    rotationRadians_.x += kAngularVelocityRadiansPerSecond.x * deltaTimeSeconds;
    rotationRadians_.y += kAngularVelocityRadiansPerSecond.y * deltaTimeSeconds;
    rotationRadians_.z += kAngularVelocityRadiansPerSecond.z * deltaTimeSeconds;
    boss.segments_[0].object->SetRotate(rotationRadians_);

    // 頭部の座標だけを更新するため、Object3dの更新を呼ぶ
    boss.segments_[0].object->SetTranslate(boss.segments_[0].position);
    boss.segments_[0].object->Update();

    // 落下時間（1.6秒）が経過するか、十分に下へ落ちたら演出を完了する
    if (elapsedSeconds_ >= kDeathFallbackDurationSeconds || boss.segments_[0].position.y < kDeathFinishHeight) {
        isDeathSequenceFinished_ = true;
    }
}

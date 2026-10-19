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
    boss.isDeathSequenceFinished_ = false;
    boss.deathSequenceTimer_ = 0.0f;

    // 胴体セグメントは即座にすべて非表示にする
    for (size_t index = 1; index < boss.segments_.size(); ++index) {
        boss.segments_[index].isAlive = false;
    }

    // 頭部の位置で大爆発エフェクトを即座に再生
    EffectManager::GetInstance()->PlayEffect("WormDeathExplosion", boss.segments_[0].position);

    // 落下物理パラメータの初期化
    // 前方（Zマイナス方向＝手前）へ飛び出しながら、上方向にポンと跳ね上がる初速を設定
    boss.deathVelocity_ = { 0.0f, 11.5f, -9.0f };
    // 落下中にクルクルと回転させる回転速度を設定
    boss.deathRotation_ = { 1.8f, 2.5f, 0.8f };
    boss.currentDeathRotation_ = boss.segments_[0].object->GetRotate();
}

void FearWormDeadState::Update(FearWormEnemy& boss)
{
    if (boss.isDeathSequenceFinished_) {
        return;
    }

    const float deltaTime = TimeManager::GetInstance()->GetDeltaTime();
    boss.deathSequenceTimer_ += deltaTime;

    // 重力を適用して落下速度を下方向（Yマイナス）に引っ張る
    constexpr float kGravity = 26.0f; // ゲーム的な落下速度に合わせた重力加速度
    boss.deathVelocity_.y -= kGravity * deltaTime;

    // 速度を頭部座標に適用
    boss.segments_[0].position.x += boss.deathVelocity_.x * deltaTime;
    boss.segments_[0].position.y += boss.deathVelocity_.y * deltaTime;
    boss.segments_[0].position.z += boss.deathVelocity_.z * deltaTime;

    // 回転速度を頭部回転角に適用
    boss.currentDeathRotation_.x += boss.deathRotation_.x * deltaTime;
    boss.currentDeathRotation_.y += boss.deathRotation_.y * deltaTime;
    boss.currentDeathRotation_.z += boss.deathRotation_.z * deltaTime;
    boss.segments_[0].object->SetRotate(boss.currentDeathRotation_);

    // 頭部の座標だけを更新するため、Object3dの更新を呼ぶ
    boss.segments_[0].object->SetTranslate(boss.segments_[0].position);
    boss.segments_[0].object->Update();

    // 落下時間（1.6秒）が経過するか、十分に下へ落ちたら演出を完了する
    constexpr float kDeathFallbackDuration = 1.6f;
    if (boss.deathSequenceTimer_ >= kDeathFallbackDuration || boss.segments_[0].position.y < -35.0f) {
        boss.isDeathSequenceFinished_ = true;
    }
}

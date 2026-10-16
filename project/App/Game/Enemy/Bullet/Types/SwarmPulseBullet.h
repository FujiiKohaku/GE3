// 群れの敵が撃つ弾。進行方向に対して横へ揺れる軌道を計算する。
#pragma once

#include "App/Game/Enemy/Bullet/Common/EnemyBullet.h"

class SwarmPulseBullet : public EnemyBullet {
public:
    // 群れ用の弾の色や材質と、波状移動の初期状態を準備する。
    void Initialize(Model* model) override;
    // 発射位置を設定し、波状軌道の基準位置も揃える。
    void SetTranslate(const Vector3& translate) override;
    // 進行速度から、弾を揺らすための横方向などを求める。
    void SetSwarmVelocity(const Vector3& velocity);
    // 弾ごとの揺れの開始角度を設定し、弾幕にずれを付ける。
    void SetWavePhase(float phase);

protected:
    // 直進する基準位置に横方向の揺れを加えて、弾の現在位置を求める。
    void Move() override;

private:
    Vector3 pathPosition_ = { 0.0f, 0.0f, 0.0f };
    Vector3 waveSide_ = { 1.0f, 0.0f, 0.0f };
    Vector3 waveUp_ = { 0.0f, 1.0f, 0.0f };
    float waveTime_ = 0.0f;
    float wavePhase_ = 0.0f;
    float waveAmplitude_ = 0.85f;
    float waveFrequency_ = 8.0f;
};

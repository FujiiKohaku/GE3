// 通常敵の射撃と難易度設定を扱う。発射間隔・弾速・HPなどを設定から変更できる。
#pragma once

#include "App/Game/Enemy/BaseEnemy.h"
#include <cmath>
#include <cstdint>

// 通常敵の難易度設定。間隔はフレーム数、弾速は一フレームの移動量で指定する。
struct NormalEnemySettings {
    float hp = 4.0f;
    int32_t fireIntervalFrames = 120;
    float bulletSpeedPerFrame = 0.48f;
    bool isRadialShotEnabled = true;
};

// HP・弾速・発射間隔が有効な範囲かを確認し、不正な難易度設定の適用を防ぐ。
inline bool IsValidNormalEnemySettings(const NormalEnemySettings& settings)
{
    return std::isfinite(settings.hp) && settings.hp > 0.0f &&
        settings.fireIntervalFrames > 0 && std::isfinite(settings.bulletSpeedPerFrame) &&
        settings.bulletSpeedPerFrame > 0.0f;
}

class Player;
class Model;

class NormalEnemy : public BaseEnemy {
public:
    std::string_view GetAppearanceId() const override { return "NormalEnemy"; }
    // 通常敵の初期HP・弾速・発射間隔と、射撃に使う参照を設定する。
    void Initialize(
        Model* model,
        Model* bulletModel,
        Player* player);

    // 基底クラスの移動・攻撃・外見更新を使って、通常敵を一フレーム進める。
    void Update() override;
    void SetRadialShotEnabled(bool isEnabled);
    // 設定値を検証してからHP・弾速・発射間隔へ反映し、不正な設定は従来値へ戻す。
    void ApplySettings(const NormalEnemySettings& settings);

protected:
    void SetBulletSpeed(float bulletSpeed);
    void SetFireInterval(int32_t fireIntervalFrames);

private:
    // プレイヤーの前方かつ射程内にいる場合だけ発射タイマーを進める。
    void Attack() override;
    // プレイヤーへ向かう弾と、設定に応じた放射状の追加弾を発射する。
    void FireBullet();
    // 指定方向へ飛ぶ敵弾を生成し、弾管理へ引き渡す。
    void SpawnBullet(const Vector3& direction, bool shouldPlayShotSound);

private:
    Player* player_ = nullptr;

    Model* bulletModel_ = nullptr;

    int32_t fireTimer_ = 0;

    int32_t fireInterval_ = 120;

    float bulletSpeed_ = 0.48f;
    bool isRadialShotEnabled_ = true;
};

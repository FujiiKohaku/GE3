// プレイヤーに向けてペイント弾を発射する敵を扱う。
#pragma once

#include "App/Game/Enemy/BaseEnemy.h"

class Player;
class Model;

class PaintShooterEnemy : public BaseEnemy {
public:
    std::string_view GetAppearanceId() const override { return "PaintShooterEnemy"; }
    PaintShooterEnemy() = default;
    ~PaintShooterEnemy() override = default;

    // 共通の敵を初期化し、ペイント弾の発射に使うモデルと狙う相手を設定する。
    void Initialize(
        Model* model,
        Model* bulletModel,
        Player* player);

    // 共通の更新経路から移動とペイント射撃を進める。
    void Update() override;

private:
    // 射程や発射間隔を確認し、プレイヤーへペイント弾を発射する。
    void Attack() override;
    // プレイヤーを狙う方向を求め、ペイント弾を敵弾管理へ渡す。
    void FirePaintBullet();

private:
    Player* player_ = nullptr;
    Model* bulletModel_ = nullptr;

    int32_t fireTimer_ = 0;
    int32_t fireInterval_ = 90; // 発射間隔
    float bulletSpeed_ = 0.85f;
};

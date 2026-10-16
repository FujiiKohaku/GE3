// 通常敵をもとにHPと攻撃設定を変えた、耐久力の高い敵を定義する。
#include "App/Game/Enemy/ArmoredEnemy/ArmoredEnemy.h"

// 通常敵の初期化を行った後、装甲敵用の耐久力と攻撃設定へ変更する。
void ArmoredEnemy::Initialize(
    Model* model,
    Model* bulletModel,
    Player* player)
{
    NormalEnemy::Initialize(
        model,
        bulletModel,
        player);

    SetRadialShotEnabled(false);
    SetBulletSpeed(0.8f);
    SetFireInterval(60);

    SetHealth(10.0f);
    transform_.scale = { 2.6f, 2.6f, 2.6f };

    if (object_ != nullptr) {
        object_->SetScale(transform_.scale);
        object_->SetColor({ 0.38f, 0.42f, 0.50f, 1.0f });
        object_->SetEnableLighting(true);
        object_->Update();
    }
}

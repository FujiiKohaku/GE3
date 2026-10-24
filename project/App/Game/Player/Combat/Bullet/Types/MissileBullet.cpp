// ミサイルの描画・威力と航跡エフェクトを扱う。消滅時には航跡も停止する。
#include "App/Game/Player/Combat/Bullet/Types/MissileBullet.h"

#include <cmath>

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
        "MissileExplosionRing",
        position);

    EffectManager::GetInstance()->PlayEffect(
        "MissileExplosionFlame",
        position);
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

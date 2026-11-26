// 敵弾に共通する描画設定と、プレイヤーへの命中時に返す演出の種類を定義する。
#include "App/Game/Enemy/Bullet/Common/EnemyBullet.h"

// 共通の弾を初期化し、敵弾の材質や霧設定を適用する。
void EnemyBullet::Initialize(Model* model)
{
    BaseBullet::Initialize(model);
    SetEnableLighting(false);
    SetColor({ 1.0f, 0.0f, 0.0f, 1.0f });
}

void EnemyBullet::SetTranslate(const Vector3& translate)
{
    BaseBullet::SetTranslate(translate);
    trailPoints_[0] = translate;
    trailPointCount_ = 1;
}

void EnemyBullet::UpdateTrail()
{
    constexpr float kMinMovementSquared = 0.000001f;
    constexpr float kMaxStraightErrorSquared = 0.0025f;
    const Vector3& head = GetPosition();
    if (trailPointCount_ == 0) {
        trailPoints_[0] = GetPreviousPosition();
        trailPointCount_ = 1;
    }

    Vector3& lastPoint = trailPoints_[trailPointCount_ - 1];
    const Vector3 movement = head - lastPoint;
    if (Dot(movement, movement) <= kMinMovementSquared) { return; }

    if (trailPointCount_ >= 2) {
        const Vector3 previousSegment = lastPoint - trailPoints_[trailPointCount_ - 2];
        const Vector3 chord = previousSegment + movement;
        const Vector3 deviation = Cross(previousSegment, chord);
        // Keep turns and collapse near-straight segments to a single span.
        if (Dot(previousSegment, movement) > 0.0f &&
            Dot(deviation, deviation) <= kMaxStraightErrorSquared * Dot(chord, chord)) {
            lastPoint = head;
            return;
        }
    }

    if (trailPointCount_ == trailPoints_.size()) {
        for (size_t index = 1; index < trailPointCount_; ++index) {
            trailPoints_[index - 1] = trailPoints_[index];
        }
        --trailPointCount_;
    }
    trailPoints_[trailPointCount_] = head;
    ++trailPointCount_;
}

// 標準の敵弾では追加の画面演出を要求しない。
EnemyBulletHitEffect EnemyBullet::OnHitPlayer(const Vector3&)
{
    return EnemyBulletHitEffect::None;
}

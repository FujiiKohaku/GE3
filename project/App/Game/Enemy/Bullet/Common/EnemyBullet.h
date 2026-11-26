// 敵弾に共通する描画設定と、プレイヤーへの命中時に返す演出の種類を定義する。
#pragma once

#include "App/Game/Common/Bullet/BaseBullet.h"
#include <array>
#include <span>

enum class EnemyBulletHitEffect { None, Paint };

struct EnemyBulletTrailStyle {
    Vector4 color { 1.0f, 0.18f, 0.06f, 0.55f };
    float maxLength = 11.0f;
    float halfWidth = 0.23f;
};

class EnemyBullet : public BaseBullet {
public:
    static constexpr size_t kMaxTrailPoints = 8;

    virtual ~EnemyBullet() = default;

    // 共通の弾を初期化し、敵弾の材質や霧設定を適用する。
    void Initialize(Model* model) override;
    void SetTranslate(const Vector3& translate) override;
    void UpdateTrail();
    std::span<const Vector3> GetTrailPoints() const { return { trailPoints_.data(), trailPointCount_ }; }
    const EnemyBulletTrailStyle& GetTrailStyle() const { return trailStyle_; }

    // 標準の敵弾では追加の画面演出を要求しない。
    virtual EnemyBulletHitEffect OnHitPlayer(const Vector3& position);

protected:
    EnemyBulletTrailStyle trailStyle_;

private:
    std::array<Vector3, kMaxTrailPoints> trailPoints_ {};
    size_t trailPointCount_ = 0;
};

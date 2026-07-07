#pragma once
#include "Engine/math/EngineStruct.h"
#include <vector>

class Model;

// ボスの各部位（部位破壊の対象）を表す簡易コライダー構造体
struct BossCollider {
    Vector3 offset;       // ボス本体からの相対座標
    float radius = 1.0f;  // 当たり判定の半径
    float hp = 1.0f;      // この部位のHP
    bool isDestroyed = false; // 破壊されたかどうか
};

class BaseBoss {
public:
    virtual ~BaseBoss() = default;

    // 初期化、更新、描画の純粋仮想関数
    virtual void Initialize(Model* model) = 0;
    virtual void Update(const Vector3& playerPosition) = 0;
    virtual void Draw() = 0;

    // 撃破判定
    virtual bool IsDead() const = 0;

    // ダメージ適用
    virtual void ApplyDamage(float damage) {}

    // 座標のGetter/Setter
    const Vector3& GetPosition() const { return position_; }
    void SetPosition(const Vector3& position) { position_ = position; }

    const Vector3& GetRotate() const { return rotate_; }
    void SetRotate(const Vector3& rotate) { rotate_ = rotate; }

    const Vector3& GetScale() const { return scale_; }
    void SetScale(const Vector3& scale) { scale_ = scale; }

    // 各部位のコライダーリストの取得インターフェース
    virtual std::vector<BossCollider>& GetColliders() = 0;

protected:
    Vector3 position_ = { 0.0f, 0.0f, 0.0f };
    Vector3 rotate_ = { 0.0f, 0.0f, 0.0f };
    Vector3 scale_ = { 1.0f, 1.0f, 1.0f };
};

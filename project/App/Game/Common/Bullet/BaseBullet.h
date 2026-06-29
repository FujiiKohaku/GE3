#pragma once
#include "Engine/3D/Model.h"
#include "Engine/3D/Object3d.h"

#include "Engine/math/MathStruct.h"
#include <memory>
class BaseBullet {

public:
    // デストラクタ 　defaultは、コンパイラに普通のデストラクタを生成させるための指定です。勝手にやってくれるので、特に何もする必要はありません。
    virtual ~BaseBullet() = default;

    // 初期化関数
    virtual void Initialize(Model* model);

    // 更新関数
    virtual void Update();
    // 描画関数
    virtual void Draw();
    bool IsAlive() const
    {
        return isAlive_;
    }
    const Vector3& GetPosition() const
    {
        return transform_.translate;
    }
    void SetVelocity(const Vector3& velocity)
    {
        velocity_ = velocity;
    }
    int GetDamage() const
    {
        return damage_;
    }
    float GetCollisionRadius() const
    {
        return collisionRadius_;
    }
    void SetCamera(Camera* camera);
    void SetTranslate(const Vector3& translate)
    {
        transform_.translate = translate;
    }
    virtual void SetDead();

private:
    std::unique_ptr<Object3d> object_;

    Camera* camera_ = nullptr;

protected:
    EulerTransform transform_;

    Vector3 velocity_;
    // 移動処理
    virtual void Move();
    bool isAlive_ = true;
    float lifeTime_ = 0.0f;
    float maxLifeTime_ = 5.0f; // 最大寿命時間（秒）
    int damage_ = 1; // ダメージ
    float collisionRadius_ = 1.0f; // 衝突判定用の半径
};

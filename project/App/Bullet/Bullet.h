#pragma once
#include "Engine/3D/Model.h"
#include "Engine/3D/Object3d.h"
#include "Engine/Math/MathStruct.h"
#include <memory>

class Camera;

class Bullet {
public:
    void Initialize(Model* model);

    void Update();

    void Draw();

    void SetCamera(Camera* camera);

    void SetTranslate(const Vector3& translate)
    {
        transform_.translate = translate;
    }

    const Vector3& GetTranslate() const
    {
        return transform_.translate;
    }

    void SetVelocity(const Vector3& velocity)
    {
        velocity_ = velocity;
    }

    bool IsAlive() const
    {
        return isAlive_;
    }

    void SetDead()
    {
        isAlive_ = false;
    }
    Vector3 GetPosition() const
    {
        return transform_.translate;
    }

private:
    std::unique_ptr<Object3d> object_;

    EulerTransform transform_;

    Vector3 velocity_ = { 0.0f, 0.0f, 0.0f };

    Camera* camera_ = nullptr;

    bool isAlive_ = true;

    float lifeTime_ = 0.0f;

    float maxLifeTime_ = 3.0f;
};
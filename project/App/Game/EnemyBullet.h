#pragma once

#include "../../Engine/3D/Object3d.h"
#include <memory>

class Camera;
class Model;

class EnemyBullet {
public:
    void Initialize(Model* model);

    void Update();
    void Draw();

    void SetCamera(Camera* camera);

    void SetTranslate(const Vector3& translate);
    void SetVelocity(const Vector3& velocity);

    Vector3 GetPosition() const;

    bool IsAlive() const;
    void SetDead() { isAlive_ = false; }

private:
    std::unique_ptr<Object3d> object_;

    EulerTransform transform_;

    Vector3 velocity_ = {};

    Camera* camera_ = nullptr;

    bool isAlive_ = true;

    int32_t lifeTime_ = 300;
};

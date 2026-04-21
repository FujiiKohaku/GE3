#pragma once
#include "../../Engine/3D/Model.h"
#include "../../Engine/3D/Object3d.h"
#include"../../Engine/math/MathStruct.h"
#include <memory>

class Camera;

class Player {
public:
    void Initialize(Model* model);
    void Update();
    void Draw();
    void SetCamera(Camera* camera);

private:
    std::unique_ptr<Object3d> object_;
    Camera* camera_ = nullptr;

    EulerTransform transform_;
    float moveSpeed_ = 0.2f;
};
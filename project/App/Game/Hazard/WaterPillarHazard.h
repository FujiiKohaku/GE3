#pragma once

#include "Engine/Math/MathStruct.h"
#include <memory>

class Model;
class Object3d;

class WaterPillarHazard {
public:
    void Initialize(Model* planeModel, Model* cylinderModel, const Vector3& position, float triggerDistance, float delay);
    void Update(float railDistance, float deltaTime);
    void Draw();
    bool CheckCollision(const Vector3& playerPosition) const;
    bool IsFinished() const;

private:
    enum class State { Waiting, Warning, Rising, Active, Fading, Finished };
    void ApplyVisuals();

    std::unique_ptr<Object3d> warning_;
    std::unique_ptr<Object3d> pillar_;
    Vector3 position_ {};
    State state_ = State::Waiting;
    float triggerDistance_ = 0.0f;
    float activationDelay_ = 0.0f;
    float timer_ = 0.0f;
    float height_ = 40.0f;
    float radius_ = 2.8f;
};

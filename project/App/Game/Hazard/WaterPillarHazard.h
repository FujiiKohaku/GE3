#pragma once

#include "Engine/Effect/EffectManager.h"
#include "Engine/Math/MathStruct.h"

class WaterPillarRenderer;

class WaterPillarHazard {
public:
    void Initialize(WaterPillarRenderer* renderer, const Vector3& position, float triggerDistance, float delay);
    void Update(float railDistance, float deltaTime);
    void DrawPillar();
    bool CheckCollision(const Vector3& playerPosition) const;
    bool IsFinished() const;

private:
    enum class State { Waiting, Warning, Rising, Active, Fading, Finished };
    void ApplyVisuals();

    WaterPillarRenderer* renderer_ = nullptr;
    EffectHandle warningEffectHandle_ = kInvalidEffectHandle;
    Vector3 pillarScale_ {};
    Vector4 pillarColor_ {};
    Vector3 position_ {};
    State state_ = State::Waiting;
    float triggerDistance_ = 0.0f;
    float activationDelay_ = 0.0f;
    float timer_ = 0.0f;
    float height_ = 52.0f;
    float radius_ = 2.8f;
};

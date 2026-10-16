// レール上の進行に合わせて出現する水柱の、予告・表示・接触判定を扱う。
#pragma once

#include "Engine/Effect/EffectManager.h"
#include "Engine/Math/MathStruct.h"

class WaterPillarRenderer;

class WaterPillarHazard {
public:
    // 水柱の位置・出現距離・遅延時間と、描画に使う参照を記録する。
    void Initialize(WaterPillarRenderer* renderer, const Vector3& position, float triggerDistance, float delay);
    // 出現距離に到達してから、予告・上昇・維持・消失の順に状態を進める。
    void Update(float railDistance, float deltaTime);
    void DrawPillar();
    // 予告中は当たらないようにし、十分に上昇した水柱の半径と高さの範囲を調べる。
    bool CheckCollision(const Vector3& playerPosition) const;
    bool IsFinished() const;

private:
    enum class State { Waiting, Warning, Rising, Active, Fading, Finished };
    // 状態と経過時間から水柱の高さと透明度を求め、急な出現や消失を避ける。
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

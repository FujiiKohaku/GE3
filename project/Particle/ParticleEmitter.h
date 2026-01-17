#pragma once
#include "MathStruct.h"
#include <cstdint>
#include <string>

class ParticleEmitter {
public:
    // 空で作れる
    ParticleEmitter();

    // Scene::Initialize で呼ぶ用
    void Init(
        const std::string& groupName,
        const EulerTransform& transform,
        uint32_t count,
        float frequency);

    void Update();
    void Emit();

private:
    std::string name_;
    EulerTransform transform_ {};
    uint32_t count_ = 0;
    float frequency_ = 0.0f;
    float elapsedTime_ = 0.0f;
};

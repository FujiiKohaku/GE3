#pragma once
#include "Struct.h"
#include <cstdint>
#include <string>

class ParticleEmitter {
public:
    // ãÛÇ≈çÏÇÍÇÈ
    ParticleEmitter();

    // Scene::Initialize Ç≈åƒÇ‘óp
    void Init(
        const std::string& groupName,
        const Transform& transform,
        uint32_t count,
        float frequency);

    void Update();
    void Emit();

private:
    std::string name_;
    Transform transform_ {};
    uint32_t count_ = 0;
    float frequency_ = 0.0f;
    float elapsedTime_ = 0.0f;
};

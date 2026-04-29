#pragma once
#include "Engine/math/MathStruct.h"

struct Particle {
    EulerTransform transform;
    Vector3 velocity;
    Vector4 color;
    float lifeTime;
    float currentTime;
};

struct ParticleForGPU {
    Matrix4x4 WVP;
    Matrix4x4 World;
    Vector4 color;
};
struct EmitterSphere {
    Vector3 translate;
    float radius;
    uint32_t count;
    float frequency;
    float frequencyTime;
    uint32_t emit;
};
struct PerFrame {
    float time;
    float deltaTime;
};
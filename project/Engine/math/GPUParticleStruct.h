#pragma once
#include"MathStruct.h"

struct ParticleCS {
    Vector3 translate;
    Vector3 scale;
    float lifeTime;
    Vector3 velocity;
    float currentTime;
    Vector4 color;
    float rotation;
    float rotationSpeed;
    Vector2 padding;
};

struct PerView {
    Matrix4x4 viewProjection;
    Matrix4x4 billboardMatrix;
    Vector3 cameraPosition;
    float padding;
};

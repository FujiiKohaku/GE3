#pragma once
#pragma once
#include "MathStruct.h"

struct SpriteVertexData {
    Vector4 position;
    Vector2 texcoord;
};

struct SpriteMaterialConstants {
    Vector4 color;
    Matrix4x4 uvTransform;
};

struct SpriteTransform {
    Matrix4x4 WVP;
};

struct SpriteEffectParameters {
    float amplitude;
    float frequency;
    float speed;
    float phase;
    Vector2 direction;
    float strength;
    float threshold;
    Vector2 spriteSize;
    Vector2 padding;
};

struct SpriteFrameParameters {
    float elapsedTime;
    float deltaTime;
    Vector2 screenSize;
};

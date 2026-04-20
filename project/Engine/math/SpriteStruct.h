#pragma once
#pragma once
#include "MathStruct.h"

struct SpriteVertexData {
    Vector4 position;
    Vector2 texcoord;
};

struct SpriteMaterial {
    Vector4 color;
    Matrix4x4 uvTransform;
};

struct SpriteTransform {
    Matrix4x4 WVP;
};

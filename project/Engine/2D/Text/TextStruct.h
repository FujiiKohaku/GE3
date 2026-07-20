#pragma once

#include "Engine/math/MathStruct.h"

struct TextVertexData {
    Vector4 position;
    Vector2 texcoord;
};

struct TextTransformConstants {
    Matrix4x4 WVP;
};

struct TextAppearanceConstants {
    Vector4 color;
    Vector4 outlineColor;
    Vector4 shadowColor;
    Vector2 atlasSize;
    Vector2 shadowOffset;
    float outlineWidth;
    float padding[3];
};

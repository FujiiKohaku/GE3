#pragma once
#include "Engine/math/MathStruct.h"
#include <cstdint>

struct ShadowSettings {
    bool enabled = false;
    uint32_t resolution = 2048;
    float distance = 350.0f;
    float casterMargin = 220.0f;
    float strength = 0.65f;
    float depthBias = 0.00025f;
    float normalBias = 0.18f;
    float pcfRadius = 1.0f;
};

struct ShadowConstants {
    Matrix4x4 lightViewProjection {};
    Vector4 parameters {}; // inverse resolution, depth bias, normal bias, strength
    Vector4 options {}; // enabled, PCF radius, unused, unused
};
static_assert(sizeof(ShadowConstants) == 96);

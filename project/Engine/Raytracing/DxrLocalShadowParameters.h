#pragma once
#include "Engine/math/MathStruct.h"
#include <cstdint>

struct DxrLocalShadowParameters {
    uint32_t pointMask = 0;
    uint32_t spotMask = 0;
    uint32_t sampleCount = 1;
    uint32_t padding = 0;
    Vector4 controls = {}; // emitter radius, normal bias, ray bias, reserved
};

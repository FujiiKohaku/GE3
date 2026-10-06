#pragma once
#include "Engine/math/MathStruct.h"
#include <cstdint>

struct RadialBlurSettings {
    int32_t sampleCount = 32;
    float width = 0.05f;
    float impulseStrength = 0.0f;
};

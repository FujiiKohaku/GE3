#pragma once

#include "MathStruct.h"

// 平行光源データ
struct DirectionalLight {
    Vector4 color;
    Vector3 direction;
    float intensity;
};

struct PointLight {
    Vector4 color;
    Vector3 position;
    float intensity;
};
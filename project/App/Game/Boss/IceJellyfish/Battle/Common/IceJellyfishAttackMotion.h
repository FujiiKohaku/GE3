#pragma once

#include "Engine/Math/MathStruct.h"
#include <cstddef>

// 通常の触手姿勢へ、攻撃固有のモーションを重ねるための入力。
struct IceJellyfishAttackMotionContext {
    size_t tentacleIndex;
    size_t segmentIndex;
    float tipWeight;
    Vector3 jointPosition;
    float radialX;
    float radialZ;
};

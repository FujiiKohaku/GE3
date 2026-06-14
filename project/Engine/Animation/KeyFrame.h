#pragma once
#include "Engine/Math/MathStruct.h"

template <typename T>
struct Keyframe {
    float time;
    T value;
};

using KeyframeVector3 = Keyframe<Vector3>;
using KeyframeQuaternion = Keyframe<Quaternion>;

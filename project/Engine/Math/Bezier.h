#pragma once
#include "MathStruct.h"

// 3次ベジェ曲線上の位置を求める。timeは0〜1の範囲外も受け付ける。
inline Vector3 CubicBezier(
    const Vector3& startPoint,
    const Vector3& firstControlPoint,
    const Vector3& secondControlPoint,
    const Vector3& endPoint,
    float time)
{
    const float inverse = 1.0f - time;
    const float weight0 = inverse * inverse * inverse;
    const float weight1 = 3.0f * inverse * inverse * time;
    const float weight2 = 3.0f * inverse * time * time;
    const float weight3 = time * time * time;
    return {
        startPoint.x * weight0 + firstControlPoint.x * weight1 +
            secondControlPoint.x * weight2 + endPoint.x * weight3,
        startPoint.y * weight0 + firstControlPoint.y * weight1 +
            secondControlPoint.y * weight2 + endPoint.y * weight3,
        startPoint.z * weight0 + firstControlPoint.z * weight1 +
            secondControlPoint.z * weight2 + endPoint.z * weight3,
    };
}

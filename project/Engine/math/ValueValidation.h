#pragma once
#include "MathStruct.h"
#include <cmath>

namespace ValueValidation {
inline bool IsFinite(float value) { return std::isfinite(value); }
inline bool IsFinite(const Vector2& v) { return IsFinite(v.x) && IsFinite(v.y); }
inline bool IsFinite(const Vector3& v) { return IsFinite(v.x) && IsFinite(v.y) && IsFinite(v.z); }
inline bool IsFinite(const Vector4& v) { return IsFinite(v.x) && IsFinite(v.y) && IsFinite(v.z) && IsFinite(v.w); }
inline bool IsFinite(const Quaternion& v) { return IsFinite(v.x) && IsFinite(v.y) && IsFinite(v.z) && IsFinite(v.w); }
inline bool IsFinite(const Matrix4x4& matrix) {
    for (const auto& row : matrix.m) { for (float value : row) { if (!IsFinite(value)) return false; } }
    return true;
}
inline bool IsValidRotation(const Quaternion& q) {
    if (!IsFinite(q)) return false;
    const double length = double(q.x)*q.x + double(q.y)*q.y + double(q.z)*q.z + double(q.w)*q.w;
    return length > 0.000001;
}
inline bool IsValidTransform(const QuaternionTransform& t) {
    return IsFinite(t.translate) && IsFinite(t.scale) && IsValidRotation(t.rotate);
}
}

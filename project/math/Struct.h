// 基本ベクトル.行列
#pragma once
#include <cmath>
struct Vector2 {
    float x, y;
};
struct Vector3 {
    float x, y, z;
};
struct Vector4 {
    float x, y, z, w;
};
struct Matrix3x3 {
    float m[3][3];
};
struct Matrix4x4 {
    float m[4][4];
};
// ===============================
// Vector3 演算関数
// ===============================

inline Vector3 operator+(const Vector3& a, const Vector3& b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
inline Vector3 operator-(const Vector3& a, const Vector3& b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
inline Vector3 operator*(const Vector3& a, float s) { return { a.x * s, a.y * s, a.z * s }; }
inline Vector3& operator+=(Vector3& a, const Vector3& b)
{
    a.x += b.x;
    a.y += b.y;
    a.z += b.z;
    return a;
}
inline Vector3& operator-=(Vector3& a, const Vector3& b)
{
    a.x -= b.x;
    a.y -= b.y;
    a.z -= b.z;
    return a;
}
inline Vector3 Normalize(const Vector3& v)
{
    float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);

    // 長さ0ならそのまま返す（ゼロ除算を防ぐ）
    if (len == 0.0f) {
        return { 0.0f, 0.0f, 0.0f };
    }

    return { v.x / len, v.y / len, v.z / len };
}

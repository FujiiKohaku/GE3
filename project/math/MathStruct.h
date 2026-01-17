#pragma once
#include <cmath>

#include<cassert>
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
// トランスフォーム情報（位置・回転・拡縮）
struct EulerTransform {
    Vector3 scale;
    Vector3 rotate;
    Vector3 translate;
};
struct Quaternion {
    float x, y, z, w;
};
struct QuaternionTransform {
    Vector3 scale;
    Quaternion rotate;
    Vector3 translate;
};
// ===============================
// Vector3 演算関数
// ===============================

static Vector3 operator+(const Vector3& a, const Vector3& b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
static Vector3 operator-(const Vector3& a, const Vector3& b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
static Vector3 operator*(const Vector3& a, float s) { return { a.x * s, a.y * s, a.z * s }; }
static Vector3& operator+=(Vector3& a, const Vector3& b)
{
    a.x += b.x;
    a.y += b.y;
    a.z += b.z;
    return a;
}
static Vector3& operator-=(Vector3& a, const Vector3& b)
{
    a.x -= b.x;
    a.y -= b.y;
    a.z -= b.z;
    return a;
}
static Quaternion operator-(const Quaternion& q)
{
    return { -q.x, -q.y, -q.z, -q.w };
}
static Quaternion operator-(const Quaternion& a, const Quaternion& b)
{
    return {
        a.x - b.x,
        a.y - b.y,
        a.z - b.z,
        a.w - b.w
    };
}
static Quaternion operator+(const Quaternion& a, const Quaternion& b)
{
    return {
        a.x + b.x,
        a.y + b.y,
        a.z + b.z,
        a.w + b.w
    };
}
static Quaternion operator*(const Quaternion& q, float s)
{
    return {
        q.x * s,
        q.y * s,
        q.z * s,
        q.w * s
    };
}
static float Norm(const Quaternion& q)
{
    return std::sqrt(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z);
}

static Vector3 Normalize(const Vector3& v)
{
    float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);

    // 長さ0ならそのまま返す（ゼロ除算を防ぐ）
    if (len == 0.0f) {
        return { 0.0f, 0.0f, 0.0f };
    }

    return { v.x / len, v.y / len, v.z / len };
}
static Quaternion Normalize(const Quaternion& q)
{
    float len = Norm(q);
    assert(len > 0.0f);

    return {
        q.x / len,
        q.y / len,
        q.z / len,
        q.w / len
    };
}

static float Dot(const Vector3& v1, const Vector3& v2) // ok
{
    return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
}

static float Dot(const Quaternion& a, const Quaternion& b)
{
    return a.w * b.w + a.x * b.x + a.y * b.y + a.z * b.z;
}

static Vector3 Lerp(const Vector3& a, const Vector3& b, float t)
{
    // 念のため範囲を固定（0～1）
    if (t < 0.0f)
        t = 0.0f;
    if (t > 1.0f)
        t = 1.0f;

    return {
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t
    };
}
static Quaternion Slerp(const Quaternion& q0, const Quaternion& q1, float t)
{
    float dot = Dot(q0, q1);

    // 逆向きなら反転（最短ルート）
    Quaternion q1Copy = q1;
    if (dot < 0.0f) {
        dot = -dot;
        q1Copy = -q1;
    }

    // ほぼ同じ向きなら Lerp で近似
    if (dot > 0.9995f) {
        Quaternion result = q0 + (q1Copy - q0) * t;
        return Normalize(result);
    }

    float theta = acosf(dot);
    float sinTheta = sinf(theta);

    float w0 = sinf((1.0f - t) * theta) / sinTheta;
    float w1 = sinf(t * theta) / sinTheta;

    return q0 * w0 + q1Copy * w1;
}

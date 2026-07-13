#include "MathStruct.h"

#include <cassert>
#include <cmath>

Vector3 operator+(const Vector3& a, const Vector3& b)
{
    return { a.x + b.x, a.y + b.y, a.z + b.z };
}

Vector3 operator-(const Vector3& a, const Vector3& b)
{
    return { a.x - b.x, a.y - b.y, a.z - b.z };
}

Vector3 operator*(const Vector3& a, float s)
{
    return { a.x * s, a.y * s, a.z * s };
}

Vector3& operator+=(Vector3& a, const Vector3& b)
{
    a.x += b.x;
    a.y += b.y;
    a.z += b.z;
    return a;
}

Vector3& operator-=(Vector3& a, const Vector3& b)
{
    a.x -= b.x;
    a.y -= b.y;
    a.z -= b.z;
    return a;
}

Quaternion operator-(const Quaternion& q)
{
    return { -q.x, -q.y, -q.z, -q.w };
}

Quaternion operator-(const Quaternion& a, const Quaternion& b)
{
    return {
        a.x - b.x,
        a.y - b.y,
        a.z - b.z,
        a.w - b.w
    };
}

Quaternion operator+(const Quaternion& a, const Quaternion& b)
{
    return {
        a.x + b.x,
        a.y + b.y,
        a.z + b.z,
        a.w + b.w
    };
}

Quaternion operator*(const Quaternion& q, float s)
{
    return {
        q.x * s,
        q.y * s,
        q.z * s,
        q.w * s
    };
}

float Vector3LengthSquared(const Vector3& v)
{
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

float Vector3Length(const Vector3& v)
{
    return std::sqrt(Vector3LengthSquared(v));
}

float Norm(const Quaternion& q)
{
    return std::sqrt(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z);
}

Vector3 Normalize(const Vector3& v)
{
    float len = Vector3Length(v);

    if (len == 0.0f) {
        return { 0.0f, 0.0f, 0.0f };
    }

    return { v.x / len, v.y / len, v.z / len };
}

Vector3 NormalizeSafe(const Vector3& v)
{
    float len = Vector3Length(v);

    if (len <= 0.0001f) {
        return { 0.0f, 0.0f, 1.0f };
    }

    return { v.x / len, v.y / len, v.z / len };
}

Quaternion Normalize(const Quaternion& q)
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

float Dot(const Vector3& v1, const Vector3& v2)
{
    return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
}

float Dot(const Quaternion& a, const Quaternion& b)
{
    return a.w * b.w + a.x * b.x + a.y * b.y + a.z * b.z;
}

Vector3 Lerp(const Vector3& a, const Vector3& b, float t)
{
    if (t < 0.0f) {
        t = 0.0f;
    }
    if (t > 1.0f) {
        t = 1.0f;
    }

    return {
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t
    };
}

Quaternion Slerp(const Quaternion& q0, const Quaternion& q1, float t)
{
    float dot = Dot(q0, q1);

    Quaternion q1Copy = q1;
    if (dot < 0.0f) {
        dot = -dot;
        q1Copy = -q1;
    }

    if (dot > 0.9995f) {
        Quaternion result = q0 + (q1Copy - q0) * t;
        return Normalize(result);
    }

    float theta = std::acos(dot);
    float sinTheta = std::sin(theta);
    float w0 = std::sin((1.0f - t) * theta) / sinTheta;
    float w1 = std::sin(t * theta) / sinTheta;

    return q0 * w0 + q1Copy * w1;
}

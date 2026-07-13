#pragma once

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

Vector3 operator+(const Vector3& a, const Vector3& b);
Vector3 operator-(const Vector3& a, const Vector3& b);
Vector3 operator*(const Vector3& a, float s);
Vector3& operator+=(Vector3& a, const Vector3& b);
Vector3& operator-=(Vector3& a, const Vector3& b);

Quaternion operator-(const Quaternion& q);
Quaternion operator-(const Quaternion& a, const Quaternion& b);
Quaternion operator+(const Quaternion& a, const Quaternion& b);
Quaternion operator*(const Quaternion& q, float s);

float Vector3LengthSquared(const Vector3& v);
float Vector3Length(const Vector3& v);
float Norm(const Quaternion& q);

Vector3 Normalize(const Vector3& v);
Vector3 NormalizeSafe(const Vector3& v);
Quaternion Normalize(const Quaternion& q);

float Dot(const Vector3& v1, const Vector3& v2);
float Dot(const Quaternion& a, const Quaternion& b);

Vector3 Lerp(const Vector3& a, const Vector3& b, float t);
Quaternion Slerp(const Quaternion& q0, const Quaternion& q1, float t);

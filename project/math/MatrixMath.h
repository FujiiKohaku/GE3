// MatrixMath.h
#pragma once
#include "MathStruct.h"
#include "MathStruct.h"
class MatrixMath {

public:
    static Matrix4x4 MakeIdentity4x4();
    static Matrix4x4 Matrix4x4MakeScaleMatrix(const Vector3& s);
    static Matrix4x4 MakeRotateXMatrix(float radian);
    static Matrix4x4 MakeRotateYMatrix(float radian);
    static Matrix4x4 MakeRotateZMatrix(float radian);
    static Matrix4x4 MakeTranslateMatrix(const Vector3& t);
    static Matrix4x4 Multiply(const Matrix4x4& m1, const Matrix4x4& m2);
    static Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translate);
    static Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Quaternion& rotate, const Vector3& translate);
    static Matrix4x4 Inverse(Matrix4x4 m);
    static Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspectRatio, float nearClip, float farClip);
    static Matrix4x4 MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip);
    static Matrix4x4 MakeViewportMatrix(float left, float top, float width, float height, float minDepth, float maxDepth);
    static Matrix4x4 Transpose(const Matrix4x4& m);
    
};


#pragma once
#include "KamataEngine.h"

/// AL3サンプルプログラム用の数学ライブラリ。
/// MT3準拠で、KamataEngine内部の数学ライブラリと重複する。
/*
struct AlmaMatrix4x4 final {
    float m[4][4];
};

struct AlmaVector4 final {
    float x;
    float y;
    float z;
    float w;
};

struct AlmaVector3 final {
    float x;
    float y;
    float z;
};

struct AlmaVector2 final {
    float x;
    float y;
};
*/
using namespace KamataEngine;

// 円周率
const float AlmaPI = 3.141592654f;

struct AlmaAABB {
	AlmaVector3 min;
	AlmaVector3 max;
};

// 02_14 29枚目 単項演算子オーバーロード
AlmaVector3 operator+(const AlmaVector3& v);
AlmaVector3 operator-(const AlmaVector3& v);

// 02_06のCameraControllerのUpdate/Reset関数で必要
const AlmaVector3 operator+(const AlmaVector3& lhv, const AlmaVector3& rhv);

// 02_06のスライド24枚目のLerp関数
AlmaVector3 AlmaLerp(const AlmaVector3& v1, const AlmaVector3& v2, float t);

// 02_06 スライド29枚目で追加
const AlmaVector3 operator*(const AlmaVector3& v1, const float f);

// 代入演算子オーバーロード
AlmaVector3& operator+=(AlmaVector3& lhs, const AlmaVector3& rhv);
AlmaVector3& operator-=(AlmaVector3& lhs, const AlmaVector3& rhv);
AlmaVector3& operator*=(AlmaVector3& v, float s);
AlmaVector3& operator/=(AlmaVector3& v, float s);

// 単位行列の作成
AlmaMatrix4x4 AlmaMakeIdentityMatrix();
// スケーリング行列の作成
AlmaMatrix4x4 AlmaMakeScaleMatrix(const AlmaVector3& scale);
// 回転行列の作成
AlmaMatrix4x4 AlmaMakeRotateXMatrix(float theta);
AlmaMatrix4x4 AlmaMakeRotateYMatrix(float theta);
AlmaMatrix4x4 AlmaMakeRotateZMatrix(float theta);
// 平行移動行列の作成
AlmaMatrix4x4 AlmaMakeTranslateMatrix(const AlmaVector3& translate);
// アフィン変換行列の作成
AlmaMatrix4x4 AlmaMakeAffineMatrix(const AlmaVector3& scale, const AlmaVector3& rot, const AlmaVector3& translate);

// 代入演算子オーバーロード
AlmaMatrix4x4& operator*=(AlmaMatrix4x4& lhm, const AlmaMatrix4x4& rhm);

// 2項演算子オーバーロード
AlmaMatrix4x4 operator*(const AlmaMatrix4x4& m1, const AlmaMatrix4x4& m2);

// ワールドトランスフォーム更新(02_03の最後)
void AlmaWorldTransformUpdate(AlmaWorldTransform& worldTransform);

float AlmaLerp(float x1, float x2, float t);

float AlmaEaseIn(float x1, float x2, float t);

float AlmaEaseOut(float x1, float x2, float t);

float AlmaEaseInOut(float x1, float x2, float t);

bool AlmaIsCollision(const AlmaAABB& aabb1, const AlmaAABB& aabb2);

AlmaVector3 AlmaTransform(const AlmaVector3& vector, const AlmaMatrix4x4& matrix);

// 02_15 で追加
inline float AlmaToRadians(float degrees) { return degrees * (3.1415f / 180.0f); }
inline float AlmaToDegrees(float radians) { return radians * (180.0f / 3.1415f); }

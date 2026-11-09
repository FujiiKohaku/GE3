
#include "Math.h"
#include <cmath>
#include <numbers>
#include <cassert>

// 02_14 29枚目 単項演算子オーバーロード
AlmaVector3 operator+(const AlmaVector3& v) { return v; }
AlmaVector3 operator-(const AlmaVector3& v) { return AlmaVector3(-v.x, -v.y, -v.z); }

// 02_06の29枚目(CameraControllerのUpdate)で必要
const AlmaVector3 operator*(const AlmaVector3& v1, const float f) {
	AlmaVector3 temp(v1);
	return temp *= f;
}

// 02_06のCameraControllerのUpdate/Reset関数で必要
const AlmaVector3 operator+(const AlmaVector3& v1, const AlmaVector3& v2) {
	AlmaVector3 temp(v1);
	return temp += v2;
}

// 02_06のスライド24枚目のLerp関数
AlmaVector3 AlmaLerp(const AlmaVector3& v1, const AlmaVector3& v2, float t) { return AlmaVector3(AlmaLerp(v1.x, v2.x, t), AlmaLerp(v1.y, v2.y, t), AlmaLerp(v1.z, v2.z, t)); }

AlmaVector3& operator+=(AlmaVector3& lhv, const AlmaVector3& rhv) {
	lhv.x += rhv.x;
	lhv.y += rhv.y;
	lhv.z += rhv.z;
	return lhv;
}

AlmaVector3& operator-=(AlmaVector3& lhv, const AlmaVector3& rhv) {
	lhv.x -= rhv.x;
	lhv.y -= rhv.y;
	lhv.z -= rhv.z;
	return lhv;
}

AlmaVector3& operator*=(AlmaVector3& v, float s) {
	v.x *= s;
	v.y *= s;
	v.z *= s;
	return v;
}

AlmaVector3& operator/=(AlmaVector3& v, float s) {
	v.x /= s;
	v.y /= s;
	v.z /= s;
	return v;
}

AlmaMatrix4x4 AlmaMakeIdentityMatrix() {
	static const AlmaMatrix4x4 result{1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};

	return result;
}

AlmaMatrix4x4 AlmaMakeScaleMatrix(const AlmaVector3& scale) {

	AlmaMatrix4x4 result{scale.x, 0.0f, 0.0f, 0.0f, 0.0f, scale.y, 0.0f, 0.0f, 0.0f, 0.0f, scale.z, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};

	return result;
}

AlmaMatrix4x4 AlmaMakeRotateXMatrix(float theta) {
	float sin = std::sin(theta);
	float cos = std::cos(theta);

	AlmaMatrix4x4 result{1.0f, 0.0f, 0.0f, 0.0f, 0.0f, cos, sin, 0.0f, 0.0f, -sin, cos, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};

	return result;
}

AlmaMatrix4x4 AlmaMakeRotateYMatrix(float theta) {
	float sin = std::sin(theta);
	float cos = std::cos(theta);

	AlmaMatrix4x4 result{cos, 0.0f, -sin, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, sin, 0.0f, cos, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};

	return result;
}

AlmaMatrix4x4 AlmaMakeRotateZMatrix(float theta) {
	float sin = std::sin(theta);
	float cos = std::cos(theta);

	AlmaMatrix4x4 result{cos, sin, 0.0f, 0.0f, -sin, cos, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};

	return result;
}

AlmaMatrix4x4 AlmaMakeTranslateMatrix(const AlmaVector3& translate) {
	AlmaMatrix4x4 result{1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, translate.x, translate.y, translate.z, 1.0f};

	return result;
}

AlmaMatrix4x4 AlmaMakeAffineMatrix(const AlmaVector3& scale, const AlmaVector3& rot, const AlmaVector3& translate) {

	// スケーリング行列の作成
	AlmaMatrix4x4 matScale = AlmaMakeScaleMatrix(scale);

	AlmaMatrix4x4 matRotX = AlmaMakeRotateXMatrix(rot.x);
	AlmaMatrix4x4 matRotY = AlmaMakeRotateYMatrix(rot.y);
	AlmaMatrix4x4 matRotZ = AlmaMakeRotateZMatrix(rot.z);
	// 回転行列の合成
	AlmaMatrix4x4 matRot = matRotZ * matRotX * matRotY;

	// 平行移動行列の作成
	AlmaMatrix4x4 matTrans = AlmaMakeTranslateMatrix(translate);

	// スケーリング、回転、平行移動の合成
	AlmaMatrix4x4 matTransform = matScale * matRot * matTrans;

	return matTransform;
}

AlmaMatrix4x4& operator*=(AlmaMatrix4x4& lhm, const AlmaMatrix4x4& rhm) {
	AlmaMatrix4x4 result{};

	for (size_t i = 0; i < 4; i++) {
		for (size_t j = 0; j < 4; j++) {
			for (size_t k = 0; k < 4; k++) {
				result.m[i][j] += lhm.m[i][k] * rhm.m[k][j];
			}
		}
	}
	lhm = result;
	return lhm;
}

AlmaMatrix4x4 operator*(const AlmaMatrix4x4& m1, const AlmaMatrix4x4& m2) {
	AlmaMatrix4x4 result = m1;

	return result *= m2;
}

// ワールドトランスフォーム更新(02_03の最後)
void AlmaWorldTransformUpdate(AlmaWorldTransform& worldTransform) {

	AlmaMatrix4x4 affin_mat = AlmaMakeAffineMatrix(worldTransform.scale_, worldTransform.rotation_, worldTransform.translation_);

	worldTransform.matWorld_ = affin_mat;

	// 定数バッファに転送する
	worldTransform.TransferMatrix();
}

float AlmaLerp(float x1, float x2, float t) { return (1.0f - t) * x1 + t * x2; }

float AlmaEaseIn(float x1, float x2, float t) {
	float easedT = t * t;

	return AlmaLerp(x1, x2, easedT);
}

float AlmaEaseOut(float x1, float x2, float t) {
	float easedT = 1.0f - std::powf(1.0f - t, 3.0f);

	return AlmaLerp(x1, x2, easedT);
}

float AlmaEaseInOut(float x1, float x2, float t) {
	float easedT = -(std::cosf(std::numbers::pi_v<float> * t) - 1.0f) / 2.0f;

	return AlmaLerp(x1, x2, easedT);
}

bool AlmaIsCollision(const AlmaAABB& aabb1, const AlmaAABB& aabb2) {
	return (aabb1.min.x <= aabb2.max.x && aabb1.max.x >= aabb2.min.x) && // x軸
	       (aabb1.min.y <= aabb2.max.y && aabb1.max.y >= aabb2.min.y) && // y軸
	       (aabb1.min.z <= aabb2.max.z && aabb1.max.z >= aabb2.min.z);   // z軸
}

AlmaVector3 AlmaTransform(const AlmaVector3& vector, const AlmaMatrix4x4& matrix) {
	AlmaVector3 result; // w=1がデカルト座標系であるので(x,y,1)のベクトルとしてmatrixとの積をとる
	result.x = vector.x * matrix.m[0][0] + vector.y * matrix.m[1][0] + vector.z * matrix.m[2][0] + 1.0f * matrix.m[3][0];
	result.y = vector.x * matrix.m[0][1] + vector.y * matrix.m[1][1] + vector.z * matrix.m[2][1] + 1.0f * matrix.m[3][1];
	result.z = vector.x * matrix.m[0][2] + vector.y * matrix.m[1][2] + vector.z * matrix.m[2][2] + 1.0f * matrix.m[3][2];
	float w = vector.x * matrix.m[0][3] + vector.y * matrix.m[1][3] + vector.z * matrix.m[2][3] + 1.0f * matrix.m[3][3];
	assert(w != 0.0f); // ベクトルに対して基本的な操作を行う行列でwが0になることはありえない
	// w=1がデカルト座標系であるので、w除算することで同次座標をデカルト座標に戻す
	result.x /= w;
	result.y /= w;
	result.z /= w;
	return result;
}

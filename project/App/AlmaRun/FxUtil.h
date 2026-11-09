#pragma once
#include "KamataEngine.h"
#include <algorithm>
#include <cmath>

//==================================================
// 演出用の小さなユーティリティ
//==================================================
namespace FxUtil {

// ワールド座標 → スクリーン座標 (1280x720)
inline KamataEngine::AlmaVector2 WorldToScreen(const KamataEngine::AlmaVector3& p, const KamataEngine::AlmaCamera& camera) {
	const KamataEngine::AlmaMatrix4x4& v = camera.matView;
	const KamataEngine::AlmaMatrix4x4& pr = camera.matProjection;
	// 行ベクトル × View
	float vx = p.x * v.m[0][0] + p.y * v.m[1][0] + p.z * v.m[2][0] + v.m[3][0];
	float vy = p.x * v.m[0][1] + p.y * v.m[1][1] + p.z * v.m[2][1] + v.m[3][1];
	float vz = p.x * v.m[0][2] + p.y * v.m[1][2] + p.z * v.m[2][2] + v.m[3][2];
	float vw = p.x * v.m[0][3] + p.y * v.m[1][3] + p.z * v.m[2][3] + v.m[3][3];
	// × Projection
	float cx = vx * pr.m[0][0] + vy * pr.m[1][0] + vz * pr.m[2][0] + vw * pr.m[3][0];
	float cy = vx * pr.m[0][1] + vy * pr.m[1][1] + vz * pr.m[2][1] + vw * pr.m[3][1];
	float cw = vx * pr.m[0][3] + vy * pr.m[1][3] + vz * pr.m[2][3] + vw * pr.m[3][3];
	if (std::fabs(cw) < 1e-5f) {
		cw = 1e-5f;
	}
	float nx = cx / cw;
	float ny = cy / cw;
	return {(nx + 1.0f) * 640.0f, (1.0f - ny) * 360.0f};
}

inline KamataEngine::AlmaVector4 LerpColor(const KamataEngine::AlmaVector4& a, const KamataEngine::AlmaVector4& b, float t) {
	return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t};
}

inline KamataEngine::AlmaVector4 MulColor(const KamataEngine::AlmaVector4& a, const KamataEngine::AlmaVector4& b) { return {a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w}; }

// 色相(0~1) → RGB（虹色演出用）
inline KamataEngine::AlmaVector4 Rainbow(float hue, float alpha = 1.0f) {
	hue = hue - std::floor(hue);
	float r = std::clamp(std::fabs(hue * 6.0f - 3.0f) - 1.0f, 0.0f, 1.0f);
	float g = std::clamp(2.0f - std::fabs(hue * 6.0f - 2.0f), 0.0f, 1.0f);
	float b = std::clamp(2.0f - std::fabs(hue * 6.0f - 4.0f), 0.0f, 1.0f);
	return {r, g, b, alpha};
}

inline float Saturate(float v) { return std::clamp(v, 0.0f, 1.0f); }

} // namespace FxUtil

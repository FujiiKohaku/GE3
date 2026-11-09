#pragma once
#include "KamataEngine.h"
#include <cstdint>

//==================================================
// バイオーム（エリア）定義
// 500m ごとに 草原 → 砂漠 → 洞窟 → 火山 とループする
//==================================================
enum class AlmaBiomeType : int {
	kGrassland = 0,
	kDesert,
	kCave,
	kVolcano,
	kCount
};

struct AlmaBiomePalette {
	const char* name;               // エリア名（英字：ビットマップフォント用）
	KamataEngine::AlmaVector4 skyTop;    // 空（上）
	KamataEngine::AlmaVector4 skyBottom; // 空（地平線）
	KamataEngine::AlmaVector4 farColor;  // 遠景
	KamataEngine::AlmaVector4 midColor;  // 中景
	KamataEngine::AlmaVector4 nearColor; // 近景
	float dayNightInfluence;         // 昼夜サイクルの影響度（屋内は小さい）
	int nearLayer;                   // 近景テクスチャ 0:木 1:サボテン 2:岩
	bool hasCeiling;                 // 洞窟の天井（つらら）を描くか
};

namespace Biome {

// 1エリアの長さ (m)
constexpr float kLength = 500.0f;
// プレイヤー開始位置 (マップチップ x=5)
constexpr float kStartOffset = 5.0f;
constexpr int kCount = static_cast<int>(AlmaBiomeType::kCount);

// 走行距離からエリア番号（ループ前の通し番号）
inline int AreaNumberAt(float distance) {
	if (distance < 0.0f) {
		distance = 0.0f;
	}
	return static_cast<int>(distance / kLength);
}

// 走行距離からバイオーム
inline int IndexAt(float distance) { return AreaNumberAt(distance) % kCount; }

// マップチップ列番号からバイオーム
inline int IndexAtColumn(uint32_t column) { return IndexAt(static_cast<float>(column) - kStartOffset); }

inline const AlmaBiomePalette& Get(int index) {
	static const AlmaBiomePalette kPalettes[kCount] = {
	    // 草原
	    {"GRASSLAND", {0.30f, 0.55f, 0.95f, 1.0f}, {0.78f, 0.92f, 1.00f, 1.0f}, {0.55f, 0.66f, 0.88f, 1.0f}, {0.38f, 0.66f, 0.42f, 1.0f}, {0.20f, 0.44f, 0.24f, 1.0f}, 1.0f, 0, false},
	    // 砂漠
	    {"DESERT", {0.95f, 0.58f, 0.30f, 1.0f}, {1.00f, 0.90f, 0.62f, 1.0f}, {0.88f, 0.62f, 0.46f, 1.0f}, {0.92f, 0.76f, 0.46f, 1.0f}, {0.62f, 0.48f, 0.26f, 1.0f}, 1.0f, 1, false},
	    // 洞窟
	    {"CRYSTAL CAVE", {0.05f, 0.04f, 0.10f, 1.0f}, {0.20f, 0.16f, 0.32f, 1.0f}, {0.22f, 0.18f, 0.34f, 1.0f}, {0.26f, 0.22f, 0.40f, 1.0f}, {0.12f, 0.10f, 0.20f, 1.0f}, 0.15f, 2, true},
	    // 火山
	    {"VOLCANO", {0.22f, 0.04f, 0.05f, 1.0f}, {0.95f, 0.38f, 0.12f, 1.0f}, {0.36f, 0.10f, 0.10f, 1.0f}, {0.50f, 0.15f, 0.08f, 1.0f}, {0.18f, 0.05f, 0.05f, 1.0f}, 0.25f, 2, false},
	};
	if (index < 0 || index >= kCount) {
		index = 0;
	}
	return kPalettes[index];
}

} // namespace Biome

#pragma once
#include "Biome.h"
#include "KamataEngine.h"
#include <array>
#include <vector>

//==================================================
// 背景（2Dスプライト）
//  - 空のグラデーション（バイオーム＋昼夜サイクル）
//  - 太陽/月・星
//  - 3層の多重スクロール（遠景/中景/近景）＋洞窟の天井
//  - 火山の火の粉・洞窟の光の粒
// ※ 3D描画の前に AlmaSprite パスで描画し、その後深度クリアする
//==================================================
class AlmaBackground {
public:
	~AlmaBackground();

	void Initialize();
	// cameraX: カメラのX座標 / distance: 走行距離 / time: 経過秒
	void Update(float cameraX, float distance, float time);
	void Draw();

	// 3Dオブジェクトに掛ける環境色（夜は青暗く、夕方はオレンジ）
	const KamataEngine::AlmaVector4& GetWorldTint() const { return worldTint_; }
	float GetNightFactor() const { return night_; }

private:
	enum Layer { kFar, kMid, kNear, kCeiling, kLayerCount };

	struct AlmaLayerInfo {
		float drawW;
		float drawH;
		float y;
		float parallax; // px / ワールド単位
	};

	struct AlmaMote {
		KamataEngine::AlmaSprite* sprite = nullptr;
		float x = 0.0f;
		float y = 0.0f;
		float speed = 0.0f;
		float size = 8.0f;
		float phase = 0.0f;
	};

	KamataEngine::AlmaSprite* CreateSprite(uint32_t tex);
	void DrawLayer(int biome, int layer, float alpha);

	// 空
	KamataEngine::AlmaSprite* skyBase_ = nullptr;
	KamataEngine::AlmaSprite* skyGrad_ = nullptr;
	KamataEngine::AlmaSprite* sun_ = nullptr;
	KamataEngine::AlmaSprite* sunGlow_ = nullptr;
	std::vector<KamataEngine::AlmaSprite*> stars_;
	std::vector<KamataEngine::AlmaVector2> starPos_;
	std::vector<float> starPhase_;

	// [biome][layer][tile]
	std::array<std::array<std::array<KamataEngine::AlmaSprite*, 2>, kLayerCount>, Biome::kCount> layers_{};
	std::array<AlmaLayerInfo, kLayerCount> layerInfo_{};

	// 火の粉・光の粒
	std::vector<AlmaMote> motes_;

	// 状態
	float cameraX_ = 0.0f;
	float time_ = 0.0f;
	int biomeA_ = 0;
	int biomeB_ = 0;
	float blend_ = 0.0f; // 0: A のみ / 1: B のみ
	float night_ = 0.0f;
	float sunset_ = 0.0f;
	float dayPhase_ = 0.0f;
	float influence_ = 1.0f;
	float moteWeightEmber_ = 0.0f;
	float moteWeightCave_ = 0.0f;
	KamataEngine::AlmaVector4 skyTop_{};
	KamataEngine::AlmaVector4 skyBottom_{};
	KamataEngine::AlmaVector4 worldTint_{1, 1, 1, 1};
	std::array<KamataEngine::AlmaVector4, kLayerCount> layerColorA_{};
	std::array<KamataEngine::AlmaVector4, kLayerCount> layerColorB_{};

	static inline const float kDayLength = 150.0f; // 昼夜1周の秒数
	static inline const float kBlendLength = 40.0f; // エリア切り替えのクロスフェード距離(m)
};

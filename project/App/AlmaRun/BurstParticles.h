#pragma once
#include "KamataEngine.h"
#include "Math.h"
#include <memory>
#include <random>
#include <vector>

//==================================================
// 汎用バーストパーティクル（プール方式）
// コイン取得のキラキラ / 敵撃破 / ブロック破片 / スーパー中の虹の軌跡 など
//==================================================
class AlmaBurstParticles {
public:
	struct AlmaEmitParams {
		KamataEngine::AlmaVector3 position{};
		int count = 8;
		KamataEngine::AlmaVector4 color{1.0f, 1.0f, 1.0f, 1.0f};
		float speedMin = 0.04f;
		float speedMax = 0.14f;
		float scale = 0.2f;
		float life = 0.6f;               // 秒
		float gravity = 0.006f;          // 1フレームあたりの落下加速度
		KamataEngine::AlmaVector3 bias{};    // 全粒子に加える初速
		uint32_t texture = 0;            // 0 ならモデル既定テクスチャ
		bool useAltModel = false;        // true: ブロック形状（破片）
		float spin = 0.0f;               // 回転速度
		bool rainbow = false;            // 粒子ごとに虹色
	};

	~AlmaBurstParticles() = default;

	void Initialize(KamataEngine::AlmaModel* model, KamataEngine::AlmaModel* altModel, KamataEngine::AlmaCamera* camera, size_t capacity = 320);
	void Emit(const AlmaEmitParams& params);
	void Update();
	void Draw();

private:
	struct AlmaParticle {
		KamataEngine::AlmaWorldTransform wt;
		KamataEngine::AlmaObjectColor color;
		KamataEngine::AlmaVector3 velocity{};
		KamataEngine::AlmaVector3 spinAxis{};
		KamataEngine::AlmaVector4 baseColor{1, 1, 1, 1};
		float life = 0.0f;
		float maxLife = 1.0f;
		float gravity = 0.0f;
		float startScale = 0.2f;
		uint32_t texture = 0;
		bool alt = false;
		bool active = false;
	};

	KamataEngine::AlmaModel* model_ = nullptr;
	KamataEngine::AlmaModel* altModel_ = nullptr;
	KamataEngine::AlmaCamera* camera_ = nullptr;
	std::vector<std::unique_ptr<AlmaParticle>> pool_;
	size_t cursor_ = 0;
	std::mt19937 rng_{20261009u};
};

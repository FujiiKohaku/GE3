#pragma once
#include "KamataEngine.h"
#include "Math.h"
#include <array>

//==================================================
// スライディング（ローリング）時の火花パーティクル
//==================================================
class AlmaSparkParticles {
public:
	// 初期化
	void Initialize(KamataEngine::AlmaModel* model, KamataEngine::AlmaCamera* camera);

	// 発生させる
	void Emit(const KamataEngine::AlmaVector3& position);

	// 更新
	void Update();

	// 描画
	void Draw();

	// 終了判定
	bool IsFinished() const { return isFinished_; }

private:
	static inline const uint32_t kNumParticles = 16; // パーティクルの数
	static inline const float kDuration_ = 0.4f;     // 寿命（秒、短くしてキレを出す）

	KamataEngine::AlmaModel* model_ = nullptr;
	KamataEngine::AlmaCamera* camera_ = nullptr;

	std::array<KamataEngine::AlmaWorldTransform, kNumParticles> worldTransforms_;
	std::array<KamataEngine::AlmaVector3, kNumParticles> velocities_;

	float counter_ = 0.0f;
	bool isFinished_ = true;

	KamataEngine::AlmaObjectColor objectColor_;
	KamataEngine::AlmaVector4 color_;
};

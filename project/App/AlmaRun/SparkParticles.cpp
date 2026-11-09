#include "SparkParticles.h"
#include <algorithm>
#include <random>

using namespace KamataEngine;

// 初期化
void AlmaSparkParticles::Initialize(AlmaModel* model, AlmaCamera* camera) {
	model_ = model;
	camera_ = camera;

	for (auto& wt : worldTransforms_) {
		wt.Initialize();
	}

	objectColor_.Initialize();
	color_ = {1.0f, 0.7f, 0.2f, 1.0f}; // ゴールド（輝く黄色）
	isFinished_ = true;
}

// 発生
void AlmaSparkParticles::Emit(const AlmaVector3& position) {
	counter_ = 0.0f;
	isFinished_ = false;

	static std::mt19937 rng(std::random_device{}());
	// スライディング（進行方向は右）なので、後方（Xマイナス方向）かつ上方向にランダムに吹き飛ばす
	std::uniform_real_distribution<float> speedDistX(-0.15f, -0.05f);
	std::uniform_real_distribution<float> speedDistY(0.02f, 0.08f);
	std::uniform_real_distribution<float> speedDistZ(-0.03f, 0.03f);

	for (uint32_t i = 0; i < kNumParticles; i++) {
		worldTransforms_[i].Initialize();
		worldTransforms_[i].translation_ = position + AlmaVector3(0.0f, 0.05f, 0.0f);
		worldTransforms_[i].scale_ = {0.12f, 0.12f, 0.12f}; // 火花なので小さく

		velocities_[i] = {
		    speedDistX(rng),
		    speedDistY(rng),
		    speedDistZ(rng)
		};
	}
}

// 更新
void AlmaSparkParticles::Update() {
	if (isFinished_)
		return;

	counter_ += 1.0f / 60.0f;
	if (counter_ >= kDuration_) {
		counter_ = kDuration_;
		isFinished_ = true;
	}

	for (uint32_t i = 0; i < kNumParticles; i++) {
		// 移動
		worldTransforms_[i].translation_ += velocities_[i];

		// 重力と減衰
		velocities_[i].y -= 0.005f; // 重力で下に落ちる
		velocities_[i].x *= 0.95f;
		velocities_[i].z *= 0.95f;

		// 火花は急速に縮小して消える
		worldTransforms_[i].scale_ *= 0.92f;

		AlmaWorldTransformUpdate(worldTransforms_[i]);
	}

	// 時間経過とともに赤み（熱が冷める）を帯びてフェードアウト
	float ratio = std::clamp(counter_ / kDuration_, 0.0f, 1.0f);
	color_.y = (1.0f - ratio) * 0.7f; // 緑を減らす（赤みを増す）
	color_.z = (1.0f - ratio) * 0.2f; // 青を減らす
	color_.w = 1.0f - ratio;          // アルファフェード
	objectColor_.SetColor(color_);
}

// 描画
void AlmaSparkParticles::Draw() {
	if (isFinished_)
		return;

	for (auto& wt : worldTransforms_) {
		model_->Draw(wt, *camera_, &objectColor_);
	}
}

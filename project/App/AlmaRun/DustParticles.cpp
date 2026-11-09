#include "DustParticles.h"
#include <algorithm>
#include <random>

using namespace KamataEngine;

// 初期化
void AlmaDustParticles::Initialize(AlmaModel* model, AlmaCamera* camera) {
	model_ = model;
	camera_ = camera;

	for (auto& wt : worldTransforms_) {
		wt.Initialize();
	}

	objectColor_.Initialize();
	color_ = {0.7f, 0.7f, 0.7f, 1.0f}; // 灰色っぽい色
	isFinished_ = true;
}

// 発生
void AlmaDustParticles::Emit(const AlmaVector3& position) {
	counter_ = 0.0f;
	isFinished_ = false;

	static std::mt19937 rng(std::random_device{}());
	std::uniform_real_distribution<float> angleDist(0.0f, 2.0f * std::numbers::pi_v<float>);
	std::uniform_real_distribution<float> speedDist(kMinSpeed_, kMaxSpeed_);

	for (uint32_t i = 0; i < kNumParticles; i++) {
		worldTransforms_[i].Initialize();
		worldTransforms_[i].translation_ = position + AlmaVector3(0, 0.05f, 0); // 少し浮かす
		worldTransforms_[i].scale_ = {0.25f, 0.25f, 0.25f};

		float angle = angleDist(rng);
		float speed = speedDist(rng);

		velocities_[i] = {
		    cosf(angle) * speed,
		    0.015f + (speed * 0.3f), // 上に舞い上がる成分
		    sinf(angle) * speed};
	}
}

// 着地発生
void AlmaDustParticles::EmitLand(const AlmaVector3& position) {
	counter_ = 0.0f;
	isFinished_ = false;

	static std::mt19937 rng(std::random_device{}());
	std::uniform_real_distribution<float> speedDist(kMinSpeed_ * 2.0f, kMaxSpeed_ * 3.0f);
	std::uniform_real_distribution<float> heightDist(0.01f, 0.03f);

	for (uint32_t i = 0; i < kNumParticles; i++) {
		worldTransforms_[i].Initialize();
		worldTransforms_[i].translation_ = position + AlmaVector3(0.0f, 0.05f, 0.0f);
		worldTransforms_[i].scale_ = {0.35f, 0.35f, 0.35f}; // 着地煙なので少し大きめ

		float speed = speedDist(rng);
		// 偶数は右、奇数は左へ
		float dirX = (i % 2 == 0) ? 1.0f : -1.0f;

		velocities_[i] = {
		    dirX * speed,
		    heightDist(rng),
		    0.0f // 2D風ゲームのためZ軸は抑えめ
		};
	}
}

// 更新
void AlmaDustParticles::Update() {
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

		// 減速 + 重力
		velocities_[i].x *= 0.94f;
		velocities_[i].z *= 0.94f;
		velocities_[i].y -= 0.001f;

		// スケールを少し広げる
		worldTransforms_[i].scale_ *= 1.01f;

		AlmaWorldTransformUpdate(worldTransforms_[i]);
	}

	// アルファフェード
	color_.w = std::clamp(1.0f - counter_ / kDuration_, 0.0f, 1.0f);
	objectColor_.SetColor(color_);
}

// 描画
void AlmaDustParticles::Draw() {
	if (isFinished_)
		return;

	for (auto& wt : worldTransforms_) {
		model_->Draw(wt, *camera_, &objectColor_);
	}
}

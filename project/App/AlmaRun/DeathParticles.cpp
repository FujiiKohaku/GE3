#include "DeathParticles.h"
#include <cstdlib> // rand()
#include <ctime>   // time()

void AlmaDeathParticles::Initialize(AlmaModel* model, AlmaCamera* camera, const AlmaVector3& position) {

	// モデルとカメラを退避
	model_ = model;
	camera_ = camera;

	// ランダム初期化
	std::srand((unsigned int)time(nullptr));

	// 各パーティクルの初期設定
	for (uint32_t i = 0; i < kNumParticles; ++i) {
		worldTransforms_[i].Initialize();
		worldTransforms_[i].translation_ = position;

		// ランダム方向ベクトル
		float angleH = ((float)std::rand() / RAND_MAX) * 2.0f * std::numbers::pi_v<float>; // 水平角
		float angleV = ((float)std::rand() / RAND_MAX) * std::numbers::pi_v<float> / 2.0f; // 垂直角(0〜90度)

		AlmaVector3 dir = {cosf(angleH) * cosf(angleV), sinf(angleV), sinf(angleH) * cosf(angleV)};

		// ランダム速度（0.1〜0.3くらい）
		float speed = 0.1f + ((float)std::rand() / RAND_MAX) * 0.2f;
		velocities_[i] = dir * speed;
	}

	// 色初期化
	objectColor_.Initialize();
	color_ = {1, 1, 1, 1};

	// 状態初期化
	counter_ = 0.0f;
	isFinished_ = false;
}

void AlmaDeathParticles::Update() {

	// 終了してたら何もしない
	if (isFinished_)
		return;

	// 時間経過
	counter_ += 1.0f / 60.0f;
	if (counter_ >= kDuration_) {
		counter_ = kDuration_;
		isFinished_ = true;
		return;
	}

	// パーティクルごとに移動処理
	for (uint32_t i = 0; i < kNumParticles; ++i) {
		// 重力を加える（下に落ちる）
		velocities_[i].y -= 0.01f;

		// 減速（空気抵抗）
		velocities_[i] = velocities_[i] * 0.98f;

		// 座標更新
		worldTransforms_[i].translation_ += velocities_[i];

		// 行列更新
		AlmaWorldTransformUpdate(worldTransforms_[i]);
	}

	// フェードアウト（時間でアルファを下げる）
	color_.w = std::clamp(1.0f - counter_ / kDuration_, 0.0f, 1.0f);
	objectColor_.SetColor(color_);
}

void AlmaDeathParticles::Draw() {
	if (isFinished_)
		return;

	for (auto& worldTransform : worldTransforms_) {
		model_->Draw(worldTransform, *camera_, &objectColor_);
	}
}

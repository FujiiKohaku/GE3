#include "RisingSpike.h"
#include <cmath>

using namespace KamataEngine;

void AlmaRisingSpike::Initialize(AlmaModel* model, AlmaCamera* camera, const AlmaVector3& position, AlmaMapChipField* mapChipField) {
	model_ = model;
	camera_ = camera;
	mapChipField_ = mapChipField;

	worldTransform_.Initialize();
	
	// position は通常床の上 Y=17 (床が Y=18)
	// 初期状態では、底面が床の天面に固定されたまま Yスケール 0 に縮んでいるように見せる
	worldTransform_.translation_ = position;
	baseY_ = position.y;
	
	// 初期値は非表示
	worldTransform_.scale_ = { 1.0f, 0.0f, 1.0f };
	// 底面を接地させたままスケール変更するため、中心位置を床の中に下げる
	worldTransform_.translation_.y = baseY_ - 0.5f;

	state_ = AlmaState::kHidden;
	riseProgress_ = 0.0f;
	
	soundHandleRise_ = AlmaAudio::GetInstance()->LoadWave("changeMode.wav"); // 飛び出し音として changeMode 音を再利用
	playedSound_ = false;
	color_.Initialize();
	color_.SetColor({1.0f, 0.45f, 0.4f, 1.0f});
}

void AlmaRisingSpike::Update(float playerX) {
	switch (state_) {
	case AlmaState::kHidden:
		// プレイヤーが手前 9.0 ユニット以内に近づいたら飛び出し開始
		if (playerX >= worldTransform_.translation_.x - 9.0f) {
			state_ = AlmaState::kRising;
			
			// 飛び出し音
			if (!playedSound_) {
				uint32_t handle = AlmaAudio::GetInstance()->PlayWave(soundHandleRise_);
				AlmaAudio::GetInstance()->SetVolume(handle, 0.08f); // 飛び出し音はやや控えめに
				playedSound_ = true;
			}
		}
		break;

	case AlmaState::kRising:
		// 高速でせり出す (約10フレームで完了)
		riseProgress_ += 0.1f;
		if (riseProgress_ >= 1.0f) {
			riseProgress_ = 1.0f;
			state_ = AlmaState::kActive;
		}

		// スケールと座標を同期して、底面を固定したままニョキッと縦に伸ばす
		worldTransform_.scale_.y = riseProgress_;
		worldTransform_.translation_.y = baseY_ - (1.0f - riseProgress_) * 0.5f;
		break;

	case AlmaState::kActive:
		// 出きった後は通常位置で固定
		worldTransform_.scale_.y = 1.0f;
		worldTransform_.translation_.y = baseY_;
		break;
	}

	AlmaWorldTransformUpdate(worldTransform_);
}

void AlmaRisingSpike::Draw() {
	if (model_ && camera_) {
		model_->Draw(worldTransform_, *camera_, &color_);
	}
}

AlmaAABB AlmaRisingSpike::GetAABB() const {
	// 現在のせり出し高さ (riseProgress_ * 1.0f) に応じて当たり判定の箱も伸縮
	float currentHalfHeight = riseProgress_ * 0.5f;
	AlmaAABB aabb = {
		{ worldTransform_.translation_.x - 0.4f, worldTransform_.translation_.y - currentHalfHeight, -0.5f },
		{ worldTransform_.translation_.x + 0.4f, worldTransform_.translation_.y + currentHalfHeight, 0.5f }
	};
	return aabb;
}

#include "Enemy2.h"
#include "HitEffect.h"
#include "MapChipField.h"
#include "Player.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>

void AlmaEnemy2::Initialize(KamataEngine::AlmaModel* model, KamataEngine::AlmaCamera* camera, const KamataEngine::AlmaVector3& position, AlmaMapChipField* mapChipField) {
	assert(model);
	model_ = model;
	camera_ = camera;
	mapChipField_ = mapChipField;

	worldTransform_.Initialize();
	worldTransform_.translation_ = position;

	// 初期は上に移動
	velocity_ = {0, kMoveSpeed, 0};

	behavior_ = AlmaBehavior::kNormal;
	isDead_ = false;
	deathTimer_ = 0;
	isCollisionDisabled_ = false;
	smashed_ = false;
	knockVelocity_ = {};
	color_.Initialize();
	colorTime_ = static_cast<float>(rand() % 100) * 0.1f;
}

void AlmaEnemy2::Update() {
	if (behavior_ == AlmaBehavior::kNormal) {
		static float time = 0.0f;
		time += 0.05f;

		// 上下に移動
		worldTransform_.translation_ += velocity_;

		if (worldTransform_.translation_.y > 5.0f || worldTransform_.translation_.y < 1.0f) {
			velocity_.y *= -1.0f; // 端で反転
		}

		// 見た目アニメーション（ぷよぷよ）
		float scaleX = 1.0f + 0.2f * std::sin(time * 2.0f);
		float scaleY = 1.0f + 0.2f * std::cos(time * 2.0f);
		worldTransform_.scale_ = {scaleX, scaleY, 1.0f};

	} else if (behavior_ == AlmaBehavior::kDefeated) {
		// デス演出
		deathTimer_++;
		if (smashed_) {
			// 体当たりで画面奥へ吹っ飛ぶ
			worldTransform_.translation_ += knockVelocity_;
			knockVelocity_.y -= 0.015f;
			worldTransform_.rotation_.z -= 0.45f;
			worldTransform_.rotation_.x += 0.3f;
			worldTransform_.scale_ = {1.0f, 1.0f, 1.0f};
		} else {
			worldTransform_.translation_.y += 0.05f;
			worldTransform_.rotation_.z += 0.2f;

			float t = deathTimer_ / 60.0f;
			float scale = std::max(0.0f, 1.0f - t);
			worldTransform_.scale_ = {scale, scale, scale};
		}

		if (deathTimer_ > 60) {
			isDead_ = true;
		}
	}

	AlmaWorldTransformUpdate(worldTransform_);
}

void AlmaEnemy2::Draw() {
	if (!isDead_) {
		colorTime_ += 1.0f / 60.0f;
		if (behavior_ == AlmaBehavior::kDefeated) {
			// 撃破直後は白く光る
			float f = std::max(0.0f, 1.0f - deathTimer_ / 12.0f);
			color_.SetColor({1.0f + f * 2.0f, 1.0f + f * 2.0f, 1.0f + f * 2.0f, 1.0f});
		} else {
			// 危険を示す赤みの脈動
			float pulse = 0.5f + 0.5f * std::sin(colorTime_ * 6.0f);
			color_.SetColor({1.0f, 0.75f - 0.2f * pulse, 0.8f - 0.1f * pulse, 1.0f});
		}
		model_->Draw(worldTransform_, *camera_, &color_);
	}
}

void AlmaEnemy2::Smash() {
	if (behavior_ == AlmaBehavior::kDefeated) {
		return;
	}
	behavior_ = AlmaBehavior::kDefeated;
	isCollisionDisabled_ = true;
	smashed_ = true;
	deathTimer_ = 0;
	knockVelocity_ = {0.18f, 0.28f, 0.05f};
}

AlmaAABB AlmaEnemy2::GetAABB() {
	KamataEngine::AlmaVector3 worldPos = GetWorldPosition();
	return {
	    {worldPos.x - kWidth / 2.0f, worldPos.y - kHeight / 2.0f, worldPos.z - kWidth / 2.0f},
        {worldPos.x + kWidth / 2.0f, worldPos.y + kHeight / 2.0f, worldPos.z + kWidth / 2.0f}
    };
}

KamataEngine::AlmaVector3 AlmaEnemy2::GetWorldPosition() { return {worldTransform_.matWorld_.m[3][0], worldTransform_.matWorld_.m[3][1], worldTransform_.matWorld_.m[3][2]}; }

void AlmaEnemy2::OnCollision(const AlmaPlayer* player) {
	(void)player;
}

bool AlmaEnemy2::IsDefeated() const { return (behavior_ == AlmaBehavior::kDefeated || isDead_); }

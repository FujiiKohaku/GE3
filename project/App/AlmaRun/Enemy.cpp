#define NOMINMAX
#define _USE_MATH_DEFINES
#include "Enemy.h"
#include "HitEffect.h"
#include "MapChipField.h"
#include "Player.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>

//--------------------------------------------------
// 初期化
//--------------------------------------------------
void AlmaEnemy::Initialize(KamataEngine::AlmaModel* model, KamataEngine::AlmaCamera* camera, const KamataEngine::AlmaVector3& position, AlmaMapChipField* mapChipField) {
	assert(model);
	model_ = model;
	camera_ = camera;
	mapChipField_ = mapChipField;

	// ワールド変換初期化
	worldTransform_.Initialize();
	worldTransform_.translation_ = position;

	// 初期速度（左移動）
	velocity_ = {-kWalkSpeed, 0, 0};

	// 左向きに回転
	worldTransform_.rotation_.y = -float(M_PI) / 2.0f;

	// 状態リセット
	behavior_ = AlmaBehavior::kNormal;
	isDead_ = false;
	deathTimer_ = 0;
	isCollisionDisabled_ = false;
	smashed_ = false;
	knockVelocity_ = {};
	color_.Initialize();
	colorTime_ = static_cast<float>(rand() % 100) * 0.1f;
}

//--------------------------------------------------
// 更新
//--------------------------------------------------
void AlmaEnemy::UpDate() {
	if (behavior_ == AlmaBehavior::kNormal) {
		static float time = 0.0f;
		time += 0.05f;

		// ====== ブロック衝突チェック ======
		KamataEngine::AlmaVector3 nextPos = worldTransform_.translation_ + velocity_;
		KamataEngine::AlmaVector3 checkPos = nextPos;

		if (velocity_.x > 0) {
			checkPos.x += kWidth / 2.0f; // 右移動中は右前方
		} else {
			checkPos.x -= kWidth / 2.0f; // 左移動中は左前方
		}

		auto index = mapChipField_->GetMapChipIndexSetByposition(checkPos);
		auto mapChip = mapChipField_->GetMapChipTypeByIndex(index.xIndex, index.yIndex);

		if (IsSolidChip(mapChip)) {
			// ブロックに当たる → 反転
			velocity_.x *= -1.0f;
			if (velocity_.x > 0) {
				worldTransform_.rotation_.y = float(M_PI) / 2.0f; // 右向き
			} else {
				worldTransform_.rotation_.y = -float(M_PI) / 2.0f; // 左向き
			}
		} else if (mapChip == AlmaMapChipType::kSpike) {
			// トゲに触れたら即死
			behavior_ = AlmaBehavior::kDefeated;
			isCollisionDisabled_ = true;
			deathTimer_ = 0;
		}

		// 移動
		worldTransform_.translation_.x += velocity_.x;

		// 呼吸スケール
		float sleepScaleY = 1.0f + 0.3f * std::sin(time);
		worldTransform_.scale_ = {1.0f, sleepScaleY, 1.0f};

		// 首振り
		float swing = 0.2f * std::sin(time * 2.0f);
		worldTransform_.rotation_.y += swing;

	} else if (behavior_ == AlmaBehavior::kDefeated) {
		// ====== デス演出 ======
		deathTimer_++;

		if (smashed_) {
			// 体当たりで画面奥へ吹っ飛ぶ
			worldTransform_.translation_ += knockVelocity_;
			knockVelocity_.y -= 0.015f;
			worldTransform_.rotation_.z -= 0.45f;
			worldTransform_.rotation_.x += 0.3f;
			worldTransform_.scale_ = {1.0f, 1.0f, 1.0f};
			if (deathTimer_ > 60) {
				isDead_ = true;
			}
		} else {
			worldTransform_.translation_.y += 0.05f; // 浮く
			worldTransform_.rotation_.z += 0.2f;     // 回転

			float t = deathTimer_ / 60.0f;
			float scale = std::max(0.0f, 1.0f - t);
			worldTransform_.scale_ = {scale, scale, scale};

			if (deathTimer_ > 60) {
				isDead_ = true;
			}
		}
	}

	AlmaWorldTransformUpdate(worldTransform_);
}

//--------------------------------------------------
// 描画
//--------------------------------------------------
void AlmaEnemy::Draw() {
	if (!isDead_) {
		colorTime_ += 1.0f / 60.0f;
		if (behavior_ == AlmaBehavior::kDefeated) {
			// 撃破直後は白く光る
			float f = std::max(0.0f, 1.0f - deathTimer_ / 12.0f);
			color_.SetColor({1.0f + f * 2.0f, 1.0f + f * 2.0f, 1.0f + f * 2.0f, 1.0f});
		} else {
			// 危険を示す赤みの脈動
			float pulse = 0.5f + 0.5f * std::sin(colorTime_ * 6.0f);
			color_.SetColor({1.0f, 0.75f - 0.2f * pulse, 0.75f - 0.2f * pulse, 1.0f});
		}
		model_->Draw(worldTransform_, *camera_, &color_);
	}
}

//--------------------------------------------------
// 吹っ飛ばされる
//--------------------------------------------------
void AlmaEnemy::Smash() {
	if (behavior_ == AlmaBehavior::kDefeated) {
		return;
	}
	behavior_ = AlmaBehavior::kDefeated;
	isCollisionDisabled_ = true;
	smashed_ = true;
	deathTimer_ = 0;
	knockVelocity_ = {0.18f, 0.28f, 0.05f};
}

//--------------------------------------------------
// AABB取得
//--------------------------------------------------
AlmaAABB AlmaEnemy::GetAABB() {
	KamataEngine::AlmaVector3 worldPos = GetWorldPosition();
	return {
	    {worldPos.x - kWidth / 2.0f, worldPos.y - kHeight / 2.0f, worldPos.z - kWidth / 2.0f},
        {worldPos.x + kWidth / 2.0f, worldPos.y + kHeight / 2.0f, worldPos.z + kWidth / 2.0f}
    };
}

//--------------------------------------------------
// ワールド座標取得
//--------------------------------------------------
KamataEngine::AlmaVector3 AlmaEnemy::GetWorldPosition() { return {worldTransform_.matWorld_.m[3][0], worldTransform_.matWorld_.m[3][1], worldTransform_.matWorld_.m[3][2]}; }

//--------------------------------------------------
// プレイヤー衝突時
//--------------------------------------------------
void AlmaEnemy::OnCollision(const AlmaPlayer* player) {
	(void)player;
}
bool AlmaEnemy::IsDefeated() const { return (behavior_ == AlmaBehavior::kDefeated || isDead_); }

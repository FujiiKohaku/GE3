#include "CameraController.h"
#include "Player.h"

#include <algorithm> // std::max, std::min を使用するために必要
#include <random>

void AlmaCameraController::Initialize(KamataEngine::AlmaCamera* camera) { camera_ = camera; }

void AlmaCameraController::Update() {
	// 前回のシェイクオフセットを差し引いて元の位置に戻す
	camera_->translation_ -= shakeOffset_;

	// プレイヤーの現在位置を取得
	const AlmaWorldTransform& targetWorldTransform = target_->GetWorldTransform();

	// プレイヤーの現在速度を取得
	const KamataEngine::AlmaVector3& targetvelocity = target_->GetVelocity();

	// カメラの目的地 = プレイヤー位置 + オフセット + (速度 × バイアス)
	destination_ = targetWorldTransform.translation_ + targetOffset_ + targetvelocity * kVelocityBias;

	// 補間でなめらかに目的地へ近づける（ゆったり追従）
	camera_->translation_ = AlmaLerp(camera_->translation_, destination_, kinterpolationRate);

	// 横方向：速度に応じた先読み（高速になるほど前方を広く見せる。上限付き）
	float targetLead = std::clamp(targetvelocity.x * kVelocityBias * 0.7f, -3.0f, 6.0f);
	lead_ += (targetLead - lead_) * 0.04f;
	camera_->translation_.x = targetWorldTransform.translation_.x + targetOffset_.x + lead_;

	// プレイヤーとのマージン制御（画面からはみ出さないように）
	camera_->translation_.y = std::max(camera_->translation_.y, destination_.y + targetMargin.bottom);
	camera_->translation_.y = std::min(camera_->translation_.y, destination_.y + targetMargin.top);

	// ステージ全体の移動範囲制限（外に出ないように）
	camera_->translation_.x = std::max(camera_->translation_.x, movableArea_.left);
	camera_->translation_.x = std::min(camera_->translation_.x, movableArea_.right);
	camera_->translation_.y = std::max(camera_->translation_.y, movableArea_.bottom);
	camera_->translation_.y = std::min(camera_->translation_.y, movableArea_.top);

	// シェイク更新
	if (shakeTimer_ > 0.0f) {
		shakeTimer_ -= 1.0f / 60.0f;
		if (shakeTimer_ <= 0.0f) {
			shakeTimer_ = 0.0f;
			shakeOffset_ = {0.0f, 0.0f, 0.0f};
		} else {
			// 徐々に減衰させる
			float progress = shakeTimer_ / shakeDuration_;
			float currentIntensity = shakeIntensity_ * progress;

			static std::mt19937 randGen(std::random_device{}());
			std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
			shakeOffset_.x = dist(randGen) * currentIntensity;
			shakeOffset_.y = dist(randGen) * currentIntensity;
			shakeOffset_.z = 0.0f;
		}
	}

	// シェイクオフセットを適用
	camera_->translation_ += shakeOffset_;

	// 最終的に行列を更新
	camera_->UpdateMatrix();
}

void AlmaCameraController::StartShake(float duration, float intensity) {
	shakeTimer_ = duration;
	shakeDuration_ = duration;
	shakeIntensity_ = intensity;
}

void AlmaCameraController::Reset() {
	// 前回のシェイクオフセットがあれば差し引く
	camera_->translation_ -= shakeOffset_;
	shakeOffset_ = {0.0f, 0.0f, 0.0f};
	shakeTimer_ = 0.0f;

	// 追従対象の和０ルドトランスフォームを参照
	const AlmaWorldTransform& targetWorldTransform = target_->GetWorldTransform();
	// 追従対象とオフセットからカメラの位置を計算
	camera_->translation_ = targetWorldTransform.translation_ + targetOffset_;
	lead_ = 0.0f;
}

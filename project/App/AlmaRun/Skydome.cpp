#include "Skydome.h"
#include "Math.h"

void AlmaSkydome::initialize(KamataEngine::AlmaModel* model, KamataEngine::AlmaCamera* camera) {

	
	model_ = model;

	camera_ = camera;

	worldTransform_.Initialize();
}

void AlmaSkydome::UpDate(const KamataEngine::AlmaVector3& position) {
	worldTransform_.translation_ = position;
	AlmaWorldTransformUpdate(worldTransform_);
	worldTransform_.TransferMatrix();
}

void AlmaSkydome::Draw() {

	model_->AlmaModel::Draw(worldTransform_, *camera_); // テクスチャなしで描画
}

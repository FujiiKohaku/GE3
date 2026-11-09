#pragma once
#include "KamataEngine.h"
class AlmaSkydome {
public:
	// 初期化
	void initialize(KamataEngine::AlmaModel* model, KamataEngine::AlmaCamera* camera);

	// 更新
	void UpDate(const KamataEngine::AlmaVector3& position);

	// 描画

	void Draw();

private:
	// ワールド変換データ
	KamataEngine::AlmaWorldTransform worldTransform_;
	// モデル
	KamataEngine::AlmaModel* model_ = nullptr;

	KamataEngine::AlmaCamera* camera_ = nullptr; // カメラへのポインタ
};

#pragma once
#include "KamataEngine.h"
#include "Math.h"
#include "MapChipField.h"

class AlmaRisingSpike {
public:
	enum class AlmaState {
		kHidden,
		kRising,
		kActive
	};

	void Initialize(KamataEngine::AlmaModel* model, KamataEngine::AlmaCamera* camera, const KamataEngine::AlmaVector3& position, AlmaMapChipField* mapChipField);
	void Update(float playerX);
	void Draw();

	// 状態と座標
	AlmaState GetState() const { return state_; }
	const KamataEngine::AlmaVector3& GetPosition() const { return worldTransform_.translation_; }
	
	// 衝突用の AlmaAABB (伸長段階に合わせて高さが変化)
	AlmaAABB GetAABB() const;
	
	// 被弾判定が有効かどうか (ある程度せり出したら有効にする)
	bool IsDamageActive() const { return riseProgress_ >= 0.3f; }

private:
	KamataEngine::AlmaWorldTransform worldTransform_;
	KamataEngine::AlmaModel* model_ = nullptr;
	KamataEngine::AlmaCamera* camera_ = nullptr;
	AlmaMapChipField* mapChipField_ = nullptr;

	AlmaState state_ = AlmaState::kHidden;
	KamataEngine::AlmaVector3 startPosition_; // 飛び出した後の通常中心位置 (Y=17)
	float baseY_ = 0.0f;
	float riseProgress_ = 0.0f;
	uint32_t soundHandleRise_ = 0;
	bool playedSound_ = false;

	// 危険を示す色
	KamataEngine::AlmaObjectColor color_;
};

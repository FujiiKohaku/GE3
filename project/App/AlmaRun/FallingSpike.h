#pragma once
#include "KamataEngine.h"
#include "Math.h"
#include "MapChipField.h"
#include "CameraController.h"
#include "DustParticles.h"

class AlmaFallingSpike {
public:
	enum class AlmaState {
		kWaiting,
		kShaking,
		kFalling,
		kLanded
	};

	void Initialize(KamataEngine::AlmaModel* model, KamataEngine::AlmaCamera* camera, const KamataEngine::AlmaVector3& position, AlmaMapChipField* mapChipField, AlmaCameraController* cController);
	void Update(float playerX);
	void Draw();
	void DrawWarningLines();

public:
	~AlmaFallingSpike();

	// 状態と座標
	AlmaState GetState() const { return state_; }
	const KamataEngine::AlmaVector3& GetPosition() const { return worldTransform_.translation_; }
	
	// 衝突用の AlmaAABB (落下中の被弾判定用)
	AlmaAABB GetAABB() const;

private:
	KamataEngine::AlmaWorldTransform worldTransform_;
	KamataEngine::AlmaModel* model_ = nullptr;
	KamataEngine::AlmaCamera* camera_ = nullptr;
	AlmaMapChipField* mapChipField_ = nullptr;
	AlmaCameraController* cController_ = nullptr;

	AlmaState state_ = AlmaState::kWaiting;
	KamataEngine::AlmaVector3 startPosition_;
	float targetY_ = 0.0f;
	float shakeTimer_ = 0.0f;
	float fallVelocity_ = 0.0f;
	uint32_t soundHandleCrash_ = 0;
	bool playedCrashSound_ = false;

	// ===== 演出用 =====
	AlmaDustParticles crashDust_;
	KamataEngine::AlmaModel* dustModel_ = nullptr;
	int blinkCounter_ = 0;

	// 危険を示す色（落ちてくる間は赤く点滅）
	KamataEngine::AlmaObjectColor color_;
};

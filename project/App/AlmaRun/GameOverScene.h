#pragma once
#define NOMINMAX
#include "Fade.h"
#include "KamataEngine.h"
#include "Math.h"
class AlmaGameOverScene {
public:
	~AlmaGameOverScene();

	void Initialize();
	void Update();
	void Draw();

	bool IsFinished() const { return finished_; }

	enum class AlmaResult { None, Retry, Title };
	AlmaResult GetResult() const { return result_; }
	enum class AlmaState { WaitInput, FadeOut, Finish };
	AlmaState state_;

private:
	enum class AlmaPhase { kFadeIn, kMain, kFadeOut };

	AlmaPhase phase_ = AlmaPhase::kFadeIn;
	bool finished_ = false;
	AlmaResult result_ = AlmaResult::None;
	AlmaResult pendingResult_ = AlmaResult::Title;

	AlmaFade* fade_ = nullptr;

	KamataEngine::AlmaModel* gameOverFont_ = nullptr;
	KamataEngine::AlmaModel* retryFont_ = nullptr;

	KamataEngine::AlmaCamera* camera_ = nullptr;

	KamataEngine::AlmaModel* modelSkyDome_ = nullptr;

	KamataEngine::AlmaWorldTransform worldTransformSky_;
	KamataEngine::AlmaWorldTransform worldTransformGameover_;
	KamataEngine::AlmaWorldTransform worldTransformPushTo_;

	uint32_t soundHandleGameOver_ = 0;
	uint32_t soundHandleSelect_ = 0;

	// ===== リザルト表示 =====
	KamataEngine::AlmaSprite* panel_ = nullptr;
	KamataEngine::AlmaSprite* line_ = nullptr;
	float resultTimer_ = 0.0f; // カウントアップ演出用
};

#pragma once
#define NOMINMAX
#include "Fade.h"
#include "KamataEngine.h"
#include "Math.h"

class AlmaClearScene {
public:
 ~AlmaClearScene();
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

	AlmaFade* fade_ = nullptr;

	KamataEngine::AlmaModel* clearFont_ = nullptr; // "GAME CLEAR"文字
	KamataEngine::AlmaModel* retryFont_ = nullptr; // "Press Enter"文字

	KamataEngine::AlmaCamera* camera_ = nullptr;
	KamataEngine::AlmaModel* modelSkyDome_ = nullptr;

	KamataEngine::AlmaWorldTransform worldTransformSky_;
	KamataEngine::AlmaWorldTransform worldTransformClear_;
	KamataEngine::AlmaWorldTransform worldTransformPushTo_;

	uint32_t soundHandleClear_;

	uint32_t soundHandleSelect_;
};

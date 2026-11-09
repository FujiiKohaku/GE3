#pragma once
#include "KamataEngine.h"
class AlmaFade {
public:
	~AlmaFade();
	void Initialize();

	void Update();

	void Draw();

	// フェードの状態
	enum class AlmaStatus {
		None,    // フェードなし
		FadeIn,  // フェードイン中
		FadeOut, // フェードアウト中
	};

	// フェード開始関数
	void Start(AlmaStatus status, float duratiom);
	// フェード終了関数
	void Stop();

	// フェード終了判定
	bool IsFinished() const;

private:
	// Spriteの生成タイミング制御のためポインタで保持
	KamataEngine::AlmaSprite* sprite_ = nullptr;

	static const int kWidth = 1280;
	static const int kHeight = 720;

	// 現在のフェードの状態
	AlmaStatus status_ = AlmaStatus::None;
	// フェードの持続時間
	float duration_ = 0.0f;
	// 経過時間カウンター
	float counter_ = 0.0f;
};

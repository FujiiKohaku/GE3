#pragma once
#include "KamataEngine.h"
#include <array>
#include <deque>
#include <list>
#include <string>

//==================================================
// HUD に渡す表示用データ
//==================================================
struct AlmaHudState {
	float hp = 100.0f;
	float maxHp = 100.0f;
	int coins = 0;
	int distance = 0;
	int bestDistance = 0;
	int score = 0;
	int combo = 0;
	int multiplier = 1;
	float comboTimeRatio = 0.0f;
	int speedLevel = 0;
	float hintAlpha = 0.0f;   // 操作説明の表示
	bool paused = false;
	bool visible = true;      // READY中などは一部非表示
};

//==================================================
// ゲーム中のHUD（2D）
//==================================================
class AlmaGameHUD {
public:
	~AlmaGameHUD();

	void Initialize();
	void Update(const AlmaHudState& state);
	// AlmaSprite::PreDraw ～ PostDraw の間で呼ぶ
	void Draw(const AlmaHudState& state);

	// 演出
	void AddPopup(const std::string& text, const KamataEngine::AlmaVector2& screenPos, const KamataEngine::AlmaVector4& color, float scale = 2.0f);
	void ShowBanner(const std::string& title, const std::string& sub, const KamataEngine::AlmaVector4& color);
	void Flash(const KamataEngine::AlmaVector4& color, float duration);
	void PulseCoin() { coinPulse_ = 1.0f; }
	void PulseScore() { scorePulse_ = 1.0f; }
	void PulseCombo() { comboPulse_ = 1.0f; }

private:
	struct AlmaPopup {
		std::string text;
		KamataEngine::AlmaVector2 pos;
		KamataEngine::AlmaVector4 color;
		float scale;
		float life;
		float maxLife;
	};
	struct AlmaBanner {
		std::string title;
		std::string sub;
		KamataEngine::AlmaVector4 color;
	};

	KamataEngine::AlmaSprite* MakeRect();
	void DrawRect(KamataEngine::AlmaSprite* s, float x, float y, float w, float h, const KamataEngine::AlmaVector4& color);
	void DrawIcon(KamataEngine::AlmaSprite* s, float x, float y, float size, const KamataEngine::AlmaVector4& color);

	uint32_t white_ = 0;
	// アイコン
	KamataEngine::AlmaSprite* heart_ = nullptr;
	KamataEngine::AlmaSprite* coinIcon_ = nullptr;
	// 矩形
	KamataEngine::AlmaSprite* panel_ = nullptr;
	KamataEngine::AlmaSprite* hpBg_ = nullptr;
	KamataEngine::AlmaSprite* hpTrail_ = nullptr;
	KamataEngine::AlmaSprite* hpFill_ = nullptr;
	KamataEngine::AlmaSprite* hpShine_ = nullptr;
	KamataEngine::AlmaSprite* comboBg_ = nullptr;
	KamataEngine::AlmaSprite* comboBar_ = nullptr;
	KamataEngine::AlmaSprite* flash_ = nullptr;
	KamataEngine::AlmaSprite* bannerBg_ = nullptr;
	KamataEngine::AlmaSprite* hintBg_ = nullptr;
	KamataEngine::AlmaSprite* pauseDim_ = nullptr;
	std::array<KamataEngine::AlmaSprite*, 4> lowHp_{};

	// 状態
	float time_ = 0.0f;
	float trailHp_ = 100.0f;
	float trailDelay_ = 0.0f;
	float lastHp_ = 100.0f;
	float coinPulse_ = 0.0f;
	float scorePulse_ = 0.0f;
	float comboPulse_ = 0.0f;
	float flashTimer_ = 0.0f;
	float flashDuration_ = 0.0f;
	KamataEngine::AlmaVector4 flashColor_{1, 1, 1, 1};
	std::list<AlmaPopup> popups_;
	std::deque<AlmaBanner> bannerQueue_;
	AlmaBanner banner_;
	float bannerTimer_ = -1.0f;

	static inline const float kBannerTime = 2.4f;
};

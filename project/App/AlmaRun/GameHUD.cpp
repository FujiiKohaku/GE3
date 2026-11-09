#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "GameHUD.h"
#include "ScoreManager.h"
#include "BitmapFont.h"
#include "FxUtil.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
using namespace KamataEngine;

namespace {
std::string ToStr(int v) {
	char buf[32];
	snprintf(buf, sizeof(buf), "%d", v);
	return buf;
}
} // namespace

AlmaGameHUD::~AlmaGameHUD() {
	AlmaSprite* sprites[] = {heart_, coinIcon_, panel_, hpBg_, hpTrail_, hpFill_, hpShine_, comboBg_, comboBar_, flash_, bannerBg_, hintBg_, pauseDim_};
	for (AlmaSprite* s : sprites) {
		delete s;
	}
	for (AlmaSprite* s : lowHp_) {
		delete s;
	}
}

AlmaSprite* AlmaGameHUD::MakeRect() { return AlmaSprite::Create(white_, {0.0f, 0.0f}); }

void AlmaGameHUD::Initialize() {
	white_ = AlmaTextureManager::Load("white1x1.png");
	heart_ = AlmaSprite::Create(AlmaTextureManager::Load("ui/heart.png"), {0, 0});
	coinIcon_ = AlmaSprite::Create(AlmaTextureManager::Load("ui/coin_icon.png"), {0, 0});
	heart_->SetAnchorPoint({0.5f, 0.5f});
	coinIcon_->SetAnchorPoint({0.5f, 0.5f});

	panel_ = MakeRect();
	hpBg_ = MakeRect();
	hpTrail_ = MakeRect();
	hpFill_ = MakeRect();
	hpShine_ = MakeRect();
	comboBg_ = MakeRect();
	comboBar_ = MakeRect();
	flash_ = MakeRect();
	bannerBg_ = MakeRect();
	hintBg_ = MakeRect();
	pauseDim_ = MakeRect();
	for (auto& s : lowHp_) {
		s = MakeRect();
	}
}

void AlmaGameHUD::DrawRect(AlmaSprite* s, float x, float y, float w, float h, const AlmaVector4& color) {
	if (!s || w <= 0.0f || h <= 0.0f || color.w <= 0.001f) {
		return;
	}
	s->SetPosition({x, y});
	s->SetSize({w, h});
	s->SetColor(color);
	s->Draw();
}

void AlmaGameHUD::DrawIcon(AlmaSprite* s, float x, float y, float size, const AlmaVector4& color) {
	if (!s) {
		return;
	}
	s->SetPosition({x, y});
	s->SetSize({size, size});
	s->SetColor(color);
	s->Draw();
}

void AlmaGameHUD::AddPopup(const std::string& text, const AlmaVector2& screenPos, const AlmaVector4& color, float scale) {
	AlmaPopup p;
	p.text = text;
	p.pos = screenPos;
	p.color = color;
	p.scale = scale;
	p.life = 0.9f;
	p.maxLife = 0.9f;
	popups_.push_back(p);
}

void AlmaGameHUD::ShowBanner(const std::string& title, const std::string& sub, const AlmaVector4& color) {
	AlmaBanner b{title, sub, color};
	if (bannerTimer_ <= 0.0f) {
		banner_ = b;
		bannerTimer_ = kBannerTime;
	} else {
		bannerQueue_.push_back(b);
	}
}

void AlmaGameHUD::Flash(const AlmaVector4& color, float duration) {
	flashColor_ = color;
	flashDuration_ = duration;
	flashTimer_ = duration;
}

void AlmaGameHUD::Update(const AlmaHudState& state) {
	const float dt = 1.0f / 60.0f;
	time_ += dt;

	// ダメージトレイルバー
	if (state.hp < lastHp_) {
		trailDelay_ = 0.45f;
	}
	lastHp_ = state.hp;
	if (trailDelay_ > 0.0f) {
		trailDelay_ -= dt;
	} else if (trailHp_ > state.hp) {
		trailHp_ = std::max(state.hp, trailHp_ - 40.0f * dt);
	} else {
		trailHp_ = state.hp;
	}

	coinPulse_ = std::max(0.0f, coinPulse_ - 4.0f * dt);
	scorePulse_ = std::max(0.0f, scorePulse_ - 4.0f * dt);
	comboPulse_ = std::max(0.0f, comboPulse_ - 4.0f * dt);
	if (flashTimer_ > 0.0f) {
		flashTimer_ = std::max(0.0f, flashTimer_ - dt);
	}

	// ポップアップ更新
	for (auto it = popups_.begin(); it != popups_.end();) {
		it->life -= dt;
		it->pos.y -= 28.0f * dt;
		if (it->life <= 0.0f) {
			it = popups_.erase(it);
		} else {
			++it;
		}
	}

	// バナー更新
	if (bannerTimer_ > 0.0f) {
		bannerTimer_ -= dt;
		if (bannerTimer_ <= 0.0f && !bannerQueue_.empty()) {
			banner_ = bannerQueue_.front();
			bannerQueue_.pop_front();
			bannerTimer_ = kBannerTime;
		}
	}
}

void AlmaGameHUD::Draw(const AlmaHudState& state) {
	AlmaBitmapFont* font = AlmaBitmapFont::GetInstance();
	using AlmaAlign = AlmaBitmapFont::AlmaAlign;

	const AlmaVector4 white = {1, 1, 1, 1};
	const AlmaVector4 gold = {1.0f, 0.85f, 0.25f, 1.0f};

	// ===== 低HP警告（画面の縁が赤く脈打つ） =====
	float hpRatio = state.maxHp > 0.0f ? std::clamp(state.hp / state.maxHp, 0.0f, 1.0f) : 0.0f;
	if (state.visible && hpRatio <= 0.3f && hpRatio > 0.0f) {
		float a = 0.18f + 0.14f * std::sin(time_ * 8.0f);
		AlmaVector4 red = {0.9f, 0.05f, 0.05f, a};
		DrawRect(lowHp_[0], 0, 0, 1280, 26, red);
		DrawRect(lowHp_[1], 0, 694, 1280, 26, red);
		DrawRect(lowHp_[2], 0, 26, 26, 668, red);
		DrawRect(lowHp_[3], 1254, 26, 26, 668, red);
	}

	if (state.visible) {
		// ===== 左上：HPバー =====
		const float barX = 64.0f, barY = 24.0f, barW = 240.0f, barH = 22.0f;
		DrawRect(panel_, 14, 12, 304, 76, {0.05f, 0.07f, 0.12f, 0.72f});
		DrawRect(hpBg_, barX - 2, barY - 2, barW + 4, barH + 4, {0.0f, 0.0f, 0.0f, 0.8f});
		float trailRatio = state.maxHp > 0.0f ? std::clamp(trailHp_ / state.maxHp, 0.0f, 1.0f) : 0.0f;
		DrawRect(hpTrail_, barX, barY, barW * trailRatio, barH, {0.9f, 0.85f, 0.3f, 0.9f});
		AlmaVector4 hpColor = hpRatio > 0.5f ? AlmaVector4{0.25f, 0.85f, 0.4f, 1.0f} : hpRatio > 0.25f ? AlmaVector4{0.95f, 0.75f, 0.2f, 1.0f} : AlmaVector4{0.95f, 0.25f, 0.2f, 1.0f};
		DrawRect(hpFill_, barX, barY, barW * hpRatio, barH, hpColor);
		DrawRect(hpShine_, barX, barY + 2, barW * hpRatio, 6, {1, 1, 1, 0.35f});
		float heartScale = hpRatio <= 0.3f ? 1.0f + 0.18f * std::sin(time_ * 8.0f) : 1.0f;
		DrawIcon(heart_, 38, barY + barH * 0.5f, 36 * heartScale, white);

		// コイン表示
		DrawIcon(coinIcon_, 38, 72, 30 * (1.0f + coinPulse_ * 0.25f), white);

		// ===== 上中央：距離メーター =====
		font->Draw(ToStr(state.distance) + " m", 640, 12, 3.4f, white, AlmaAlign::kCenter);
		font->Draw("BEST " + ToStr(state.bestDistance) + " m", 640, 76, 1.4f, {0.85f, 0.85f, 0.9f, 0.9f}, AlmaAlign::kCenter);
		font->Draw("SPEED LV " + ToStr(state.speedLevel + 1), 640, 100, 1.3f, {0.5f, 0.95f, 1.0f, 0.95f}, AlmaAlign::kCenter);

		// ===== 右上：スコアとコンボ =====
		font->Draw("SCORE", 1262, 14, 1.4f, {0.85f, 0.85f, 0.9f, 0.9f}, AlmaAlign::kRight);
		font->Draw(ToStr(state.score), 1262, 38, 3.0f * (1.0f + scorePulse_ * 0.12f), white, AlmaAlign::kRight);

		if (state.combo > 1) {
			int m = state.multiplier;
			AlmaVector4 mc = m >= 5 ? AlmaVector4{1.0f, 0.35f, 0.85f, 1.0f} : m >= 4 ? AlmaVector4{1.0f, 0.6f, 0.2f, 1.0f} : m >= 3 ? AlmaVector4{1.0f, 0.9f, 0.2f, 1.0f} : AlmaVector4{0.4f, 0.95f, 1.0f, 1.0f};
			float pop = 1.0f + comboPulse_ * 0.2f;
			font->Draw("x" + ToStr(m), 1262, 100, 3.2f * pop, mc, AlmaAlign::kRight);
			font->Draw(ToStr(state.combo) + " COMBO", 1262 - 9.0f * 3.2f * 2.0f - 14.0f, 112, 1.8f, mc, AlmaAlign::kRight);
			DrawRect(comboBg_, 1062, 162, 200, 6, {0, 0, 0, 0.45f});
			DrawRect(comboBar_, 1062, 162, 200 * std::clamp(state.comboTimeRatio, 0.0f, 1.0f), 6, mc);
		}

		// ===== 操作説明 =====
		if (state.hintAlpha > 0.01f) {
			float a = state.hintAlpha;
			DrawRect(hintBg_, 280, 500, 720, 84, {0, 0, 0, 0.5f * a});
			font->Draw("[UP] JUMP  (PRESS AGAIN FOR DOUBLE JUMP)", 640, 514, 1.7f, {1, 1, 1, a}, AlmaAlign::kCenter);
			font->Draw("[DOWN] ROLL : SMASH ENEMIES & CRACKED BLOCKS", 640, 548, 1.7f, {1.0f, 0.85f, 0.4f, a}, AlmaAlign::kCenter);
		}
	}

	// ===== ポップアップ =====
	for (const AlmaPopup& p : popups_) {
		float t = p.life / p.maxLife; // 1 → 0
		float age = p.maxLife - p.life;
		float pop = age < 0.12f ? 1.0f + (0.12f - age) / 0.12f * 0.6f : 1.0f;
		AlmaVector4 c = p.color;
		c.w *= std::clamp(t * 1.5f, 0.0f, 1.0f);
		font->Draw(p.text, p.pos.x, p.pos.y, p.scale * pop, c, AlmaAlign::kCenter);
	}

	// ===== 大型バナー通知 =====
	if (bannerTimer_ > 0.0f) {
		float progress = 1.0f - bannerTimer_ / kBannerTime;
		float inOut = progress < 0.18f ? progress / 0.18f : progress > 0.82f ? (1.0f - progress) / 0.18f : 1.0f;
		float alpha = std::clamp(inOut, 0.0f, 1.0f);
		float slide = (1.0f - inOut) * 80.0f;

		DrawRect(bannerBg_, 0, 220, 1280, 130, {0.03f, 0.04f, 0.08f, 0.82f * alpha});
		font->Draw(banner_.title, 640 + slide, 246, 4.0f, banner_.color, AlmaAlign::kCenter);
		if (!banner_.sub.empty()) {
			font->Draw(banner_.sub, 640 + slide * 1.2f, 312, 1.6f, {1, 1, 1, 0.95f}, AlmaAlign::kCenter);
		}
	}

	// ===== ポーズ中 =====
	if (state.paused) {
		DrawRect(pauseDim_, 0, 0, 1280, 720, {0, 0, 0, 0.45f});
	}

	// ===== 画面フラッシュ =====
	if (flashTimer_ > 0.0f && flashDuration_ > 0.0f) {
		float a = flashTimer_ / flashDuration_;
		AlmaVector4 fc = flashColor_;
		fc.w *= a;
		DrawRect(flash_, 0, 0, 1280, 720, fc);
	}
}

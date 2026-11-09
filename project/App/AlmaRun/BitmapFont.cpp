#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "BitmapFont.h"
#include <algorithm>

using namespace KamataEngine;

AlmaBitmapFont* AlmaBitmapFont::GetInstance() {
	static AlmaBitmapFont instance;
	return &instance;
}

void AlmaBitmapFont::EnsureInitialized() {
	if (initialized_) {
		return;
	}
	texture_ = AlmaTextureManager::Load("debugfont.png");
	pool_.reserve(kMaxSprites);
	initialized_ = true;
}

AlmaSprite* AlmaBitmapFont::Next() {
	if (index_ >= kMaxSprites) {
		return nullptr;
	}
	if (index_ >= pool_.size()) {
		pool_.push_back(AlmaSprite::Create(texture_, {0.0f, 0.0f}));
	}
	return pool_[index_++];
}

void AlmaBitmapFont::DrawRaw(const std::string& text, float x, float y, float scale, const AlmaVector4& color) {
	float cursor = x;
	for (char c : text) {
		int code = static_cast<unsigned char>(c);
		if (code < 32 || code > 126) {
			code = '?';
		}
		if (code != ' ') {
			AlmaSprite* sprite = Next();
			if (!sprite) {
				return;
			}
			int index = code - 32;
			float u = static_cast<float>(index % kGlyphsPerLine) * kGlyphW;
			float v = static_cast<float>(index / kGlyphsPerLine) * kGlyphH;
			sprite->SetTextureRect({u, v}, {kGlyphW, kGlyphH});
			sprite->SetSize({kGlyphW * scale, kGlyphH * scale});
			sprite->SetPosition({cursor, y});
			sprite->SetColor(color);
			sprite->Draw();
		}
		cursor += kGlyphW * scale;
	}
}

void AlmaBitmapFont::Draw(const std::string& text, float x, float y, float scale, const AlmaVector4& color, AlmaAlign align, bool shadow) {
	EnsureInitialized();
	float width = Measure(text, scale);
	if (align == AlmaAlign::kCenter) {
		x -= width * 0.5f;
	} else if (align == AlmaAlign::kRight) {
		x -= width;
	}
	if (shadow) {
		float offset = std::max(1.0f, scale * 0.9f);
		DrawRaw(text, x + offset, y + offset, scale, {0.0f, 0.0f, 0.0f, color.w * 0.65f});
	}
	DrawRaw(text, x, y, scale, color);
}

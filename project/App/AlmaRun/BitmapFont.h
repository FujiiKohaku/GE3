#pragma once
#include "KamataEngine.h"
#include <string>
#include <vector>

//==================================================
// ビットマップフォント
// debugfont.png (9x18 / 14文字×7行) を使って
// 色・拡大率・揃え・影つきで文字列を描画する
// ※ AlmaSprite::PreDraw ～ PostDraw の間で呼ぶこと
//==================================================
class AlmaBitmapFont {
public:
	enum class AlmaAlign { kLeft, kCenter, kRight };

	static AlmaBitmapFont* GetInstance();

	// 毎フレーム描画前に呼ぶ（スプライトプールの使用位置をリセット）
	void BeginFrame() { index_ = 0; }

	// 文字列描画
	void Draw(const std::string& text, float x, float y, float scale, const KamataEngine::AlmaVector4& color, AlmaAlign align = AlmaAlign::kLeft, bool shadow = true);

	// 文字列の幅
	static float Measure(const std::string& text, float scale) { return static_cast<float>(text.size()) * kGlyphW * scale; }
	static float LineHeight(float scale) { return kGlyphH * scale; }

private:
	AlmaBitmapFont() = default;
	void EnsureInitialized();
	KamataEngine::AlmaSprite* Next();
	void DrawRaw(const std::string& text, float x, float y, float scale, const KamataEngine::AlmaVector4& color);

	static inline const float kGlyphW = 9.0f;
	static inline const float kGlyphH = 18.0f;
	static inline const int kGlyphsPerLine = 14;
	static inline const size_t kMaxSprites = 1500;

	uint32_t texture_ = 0;
	bool initialized_ = false;
	std::vector<KamataEngine::AlmaSprite*> pool_;
	size_t index_ = 0;
};

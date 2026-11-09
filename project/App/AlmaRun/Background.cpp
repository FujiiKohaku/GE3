#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "Background.h"
#include "FxUtil.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <random>

using namespace KamataEngine;

namespace {
float SmoothStep(float e0, float e1, float x) {
	float t = std::clamp((x - e0) / (e1 - e0), 0.0f, 1.0f);
	return t * t * (3.0f - 2.0f * t);
}

// バイオームごとの3Dオブジェクトへの環境色
const AlmaVector4 kBiomeTint[Biome::kCount] = {
    {1.00f, 1.00f, 1.00f, 1.0f},
    {1.00f, 0.96f, 0.88f, 1.0f},
    {0.80f, 0.82f, 0.98f, 1.0f},
    {1.00f, 0.82f, 0.74f, 1.0f},
};
} // namespace

AlmaBackground::~AlmaBackground() {
	delete skyBase_;
	delete skyGrad_;
	delete sun_;
	delete sunGlow_;
	for (AlmaSprite* s : stars_) {
		delete s;
	}
	for (auto& biome : layers_) {
		for (auto& layer : biome) {
			for (AlmaSprite* s : layer) {
				delete s;
			}
		}
	}
	for (AlmaMote& m : motes_) {
		delete m.sprite;
	}
}

AlmaSprite* AlmaBackground::CreateSprite(uint32_t tex) { return AlmaSprite::Create(tex, {0.0f, 0.0f}); }

void AlmaBackground::Initialize() {
	uint32_t white = AlmaTextureManager::Load("white1x1.png");
	uint32_t grad = AlmaTextureManager::Load("ui/sky_grad.png");
	uint32_t glow = AlmaTextureManager::Load("ui/glow.png");
	uint32_t farTex = AlmaTextureManager::Load("bg/far_mountains.png");
	uint32_t midTex = AlmaTextureManager::Load("bg/mid_hills.png");
	uint32_t nearTex[3] = {AlmaTextureManager::Load("bg/near_trees.png"), AlmaTextureManager::Load("bg/near_cacti.png"), AlmaTextureManager::Load("bg/near_rocks.png")};
	uint32_t ceiling = AlmaTextureManager::Load("bg/cave_ceiling.png");

	skyBase_ = CreateSprite(white);
	skyGrad_ = CreateSprite(grad);
	sun_ = CreateSprite(glow);
	sunGlow_ = CreateSprite(glow);
	sun_->SetAnchorPoint({0.5f, 0.5f});
	sunGlow_->SetAnchorPoint({0.5f, 0.5f});

	// 星
	std::mt19937 rng(777u);
	std::uniform_real_distribution<float> sx(0.0f, 1280.0f);
	std::uniform_real_distribution<float> sy(0.0f, 380.0f);
	std::uniform_real_distribution<float> ph(0.0f, 6.28f);
	for (int i = 0; i < 70; ++i) {
		AlmaSprite* s = CreateSprite(glow);
		s->SetAnchorPoint({0.5f, 0.5f});
		float size = (i % 7 == 0) ? 14.0f : 7.0f;
		s->SetSize({size, size});
		stars_.push_back(s);
		starPos_.push_back({sx(rng), sy(rng)});
		starPhase_.push_back(ph(rng));
	}

	// 多重スクロールのレイヤー
	layerInfo_[kFar] = {1280.0f, 480.0f, 120.0f, 3.0f};
	layerInfo_[kMid] = {1280.0f, 320.0f, 330.0f, 10.0f};
	layerInfo_[kNear] = {1280.0f, 320.0f, 380.0f, 24.0f};
	layerInfo_[kCeiling] = {1280.0f, 320.0f, -30.0f, 16.0f};
	for (int b = 0; b < Biome::kCount; ++b) {
		const AlmaBiomePalette& pal = Biome::Get(b);
		for (int t = 0; t < 2; ++t) {
			layers_[b][kFar][t] = CreateSprite(farTex);
			layers_[b][kMid][t] = CreateSprite(midTex);
			layers_[b][kNear][t] = CreateSprite(nearTex[std::clamp(pal.nearLayer, 0, 2)]);
			layers_[b][kCeiling][t] = pal.hasCeiling ? CreateSprite(ceiling) : nullptr;
		}
	}

	// 火の粉 / 光の粒
	std::uniform_real_distribution<float> my(0.0f, 720.0f);
	std::uniform_real_distribution<float> ms(0.6f, 2.2f);
	std::uniform_real_distribution<float> msize(5.0f, 16.0f);
	for (int i = 0; i < 45; ++i) {
		AlmaMote m;
		m.sprite = CreateSprite(glow);
		m.sprite->SetAnchorPoint({0.5f, 0.5f});
		m.x = sx(rng);
		m.y = my(rng);
		m.speed = ms(rng);
		m.size = msize(rng);
		m.phase = ph(rng);
		motes_.push_back(m);
	}
}

void AlmaBackground::Update(float cameraX, float distance, float time) {
	cameraX_ = cameraX;
	time_ = time;

	// ===== エリアのクロスフェード =====
	int area = Biome::AreaNumberAt(distance);
	float local = std::max(0.0f, distance) - static_cast<float>(area) * Biome::kLength;
	biomeB_ = area % Biome::kCount;
	if (area > 0 && local < kBlendLength) {
		biomeA_ = (area - 1) % Biome::kCount;
		blend_ = local / kBlendLength;
	} else {
		biomeA_ = biomeB_;
		blend_ = 1.0f;
	}
	const AlmaBiomePalette& pa = Biome::Get(biomeA_);
	const AlmaBiomePalette& pb = Biome::Get(biomeB_);
	influence_ = pa.dayNightInfluence + (pb.dayNightInfluence - pa.dayNightInfluence) * blend_;

	// ===== 昼夜サイクル =====
	dayPhase_ = std::fmod(time / kDayLength + 0.05f, 1.0f);
	night_ = SmoothStep(0.45f, 0.58f, dayPhase_) * (1.0f - SmoothStep(0.88f, 1.0f, dayPhase_));
	sunset_ = std::max(std::max(0.0f, 1.0f - std::fabs(dayPhase_ - 0.5f) / 0.07f), std::max(0.0f, 1.0f - std::fabs(dayPhase_ - 0.94f) / 0.06f));

	const AlmaVector4 nightMul = {0.16f, 0.20f, 0.42f, 1.0f};
	const AlmaVector4 sunsetMul = {1.25f, 0.72f, 0.55f, 1.0f};
	auto applyTod = [&](AlmaVector4 c) {
		c = FxUtil::LerpColor(c, FxUtil::MulColor(c, nightMul), night_ * influence_);
		c = FxUtil::LerpColor(c, FxUtil::MulColor(c, sunsetMul), sunset_ * influence_);
		c.w = 1.0f;
		return c;
	};

	skyTop_ = applyTod(FxUtil::LerpColor(pa.skyTop, pb.skyTop, blend_));
	skyBottom_ = applyTod(FxUtil::LerpColor(pa.skyBottom, pb.skyBottom, blend_));

	auto layerColors = [&](const AlmaBiomePalette& p, std::array<AlmaVector4, kLayerCount>& out) {
		out[kFar] = applyTod(FxUtil::LerpColor(p.farColor, p.skyBottom, 0.25f));
		out[kMid] = applyTod(p.midColor);
		out[kNear] = applyTod(p.nearColor);
		out[kCeiling] = applyTod(FxUtil::MulColor(p.nearColor, {0.8f, 0.8f, 0.8f, 1.0f}));
	};
	layerColors(pa, layerColorA_);
	layerColors(pb, layerColorB_);

	// ===== 3Dへの環境色 =====
	AlmaVector4 tint = FxUtil::LerpColor(kBiomeTint[biomeA_], kBiomeTint[biomeB_], blend_);
	tint = FxUtil::LerpColor(tint, FxUtil::MulColor(tint, {0.55f, 0.6f, 0.9f, 1.0f}), night_ * influence_);
	tint = FxUtil::LerpColor(tint, FxUtil::MulColor(tint, {1.0f, 0.84f, 0.72f, 1.0f}), sunset_ * influence_);
	tint.w = 1.0f;
	worldTint_ = tint;

	// ===== 火の粉・光の粒 =====
	moteWeightEmber_ = (biomeA_ == static_cast<int>(AlmaBiomeType::kVolcano) ? 1.0f - blend_ : 0.0f) + (biomeB_ == static_cast<int>(AlmaBiomeType::kVolcano) ? blend_ : 0.0f);
	moteWeightCave_ = (biomeA_ == static_cast<int>(AlmaBiomeType::kCave) ? 1.0f - blend_ : 0.0f) + (biomeB_ == static_cast<int>(AlmaBiomeType::kCave) ? blend_ : 0.0f);
	if (biomeA_ == biomeB_) {
		moteWeightEmber_ = biomeB_ == static_cast<int>(AlmaBiomeType::kVolcano) ? 1.0f : 0.0f;
		moteWeightCave_ = biomeB_ == static_cast<int>(AlmaBiomeType::kCave) ? 1.0f : 0.0f;
	}
	float riseSpeed = 0.4f + 1.6f * moteWeightEmber_;
	for (AlmaMote& m : motes_) {
		m.y -= m.speed * riseSpeed;
		m.x += std::sin(time_ * 1.3f + m.phase) * 0.6f - m.speed * 0.8f;
		if (m.y < -20.0f) {
			m.y = 740.0f;
		}
		if (m.x < -20.0f) {
			m.x += 1320.0f;
		}
	}
}

void AlmaBackground::DrawLayer(int biome, int layer, float alpha) {
	if (alpha <= 0.001f) {
		return;
	}
	auto& tiles = layers_[biome][layer];
	if (!tiles[0]) {
		return;
	}
	const AlmaLayerInfo& info = layerInfo_[layer];
	float offset = std::fmod(cameraX_ * info.parallax, info.drawW);
	if (offset < 0.0f) {
		offset += info.drawW;
	}
	AlmaVector4 color = (biome == biomeA_ ? layerColorA_ : layerColorB_)[layer];
	color.w = alpha;
	for (int t = 0; t < 2; ++t) {
		AlmaSprite* s = tiles[t];
		s->SetPosition({-offset + info.drawW * static_cast<float>(t), info.y});
		s->SetSize({info.drawW + 1.0f, info.drawH});
		s->SetColor(color);
		s->Draw();
	}
}

void AlmaBackground::Draw() {
	// 空
	skyBase_->SetPosition({0.0f, 0.0f});
	skyBase_->SetSize({1280.0f, 720.0f});
	skyBase_->SetColor(skyTop_);
	skyBase_->Draw();
	skyGrad_->SetPosition({0.0f, 0.0f});
	skyGrad_->SetSize({1280.0f, 720.0f});
	skyGrad_->SetColor(skyBottom_);
	skyGrad_->Draw();

	// 星
	float starAlpha = night_ * std::clamp(influence_, 0.0f, 1.0f);
	if (starAlpha > 0.01f) {
		for (size_t i = 0; i < stars_.size(); ++i) {
			float tw = 0.55f + 0.45f * std::sin(time_ * 2.2f + starPhase_[i]);
			float x = std::fmod(starPos_[i].x - cameraX_ * 0.5f, 1280.0f);
			if (x < 0.0f) {
				x += 1280.0f;
			}
			stars_[i]->SetPosition({x, starPos_[i].y});
			stars_[i]->SetColor({1.0f, 1.0f, 0.95f, starAlpha * tw});
			stars_[i]->Draw();
		}
	}

	// 太陽 / 月
	{
		bool isSun = dayPhase_ < 0.5f;
		float t = isSun ? dayPhase_ / 0.5f : (dayPhase_ - 0.5f) / 0.5f;
		float x = 1150.0f - t * 1020.0f;
		float y = 330.0f - std::sin(t * std::numbers::pi_v<float>) * 250.0f;
		float alpha = std::clamp(std::sin(t * std::numbers::pi_v<float>) * 2.0f, 0.0f, 1.0f) * std::clamp(influence_, 0.0f, 1.0f);
		if (alpha > 0.01f) {
			AlmaVector4 core = isSun ? AlmaVector4{1.0f, 0.95f, 0.75f, alpha} : AlmaVector4{0.9f, 0.95f, 1.0f, alpha};
			AlmaVector4 halo = isSun ? AlmaVector4{1.0f, 0.8f, 0.4f, alpha * 0.45f} : AlmaVector4{0.6f, 0.7f, 1.0f, alpha * 0.3f};
			float coreSize = isSun ? 150.0f : 90.0f;
			sunGlow_->SetPosition({x, y});
			sunGlow_->SetSize({coreSize * 3.0f, coreSize * 3.0f});
			sunGlow_->SetColor(halo);
			sunGlow_->Draw();
			sun_->SetPosition({x, y});
			sun_->SetSize({coreSize, coreSize});
			sun_->SetColor(core);
			sun_->Draw();
		}
	}

	// 多重スクロール（奥から順に）
	const int order[] = {kFar, kMid, kCeiling, kNear};
	for (int layer : order) {
		DrawLayer(biomeA_, layer, 1.0f);
		if (biomeB_ != biomeA_) {
			DrawLayer(biomeB_, layer, blend_);
		}
	}

	// 火の粉 / 光の粒
	float moteAlpha = std::max(moteWeightEmber_, moteWeightCave_);
	if (moteAlpha > 0.01f) {
		AlmaVector4 ember = {1.0f, 0.55f, 0.15f, 1.0f};
		AlmaVector4 cave = {0.45f, 1.0f, 0.9f, 1.0f};
		float sum = moteWeightEmber_ + moteWeightCave_;
		AlmaVector4 c = FxUtil::LerpColor(cave, ember, sum > 0.0f ? moteWeightEmber_ / sum : 0.0f);
		for (AlmaMote& m : motes_) {
			float flicker = 0.5f + 0.5f * std::sin(time_ * 5.0f + m.phase * 3.0f);
			c.w = moteAlpha * (0.35f + 0.65f * flicker);
			m.sprite->SetPosition({m.x, m.y});
			m.sprite->SetSize({m.size, m.size});
			m.sprite->SetColor(c);
			m.sprite->Draw();
		}
	}
}

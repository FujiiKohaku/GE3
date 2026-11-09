#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "BurstParticles.h"
#include "FxUtil.h"
#include <algorithm>
#include <cmath>
#include <numbers>

using namespace KamataEngine;

void AlmaBurstParticles::Initialize(AlmaModel* model, AlmaModel* altModel, AlmaCamera* camera, size_t capacity) {
	model_ = model;
	altModel_ = altModel ? altModel : model;
	camera_ = camera;
	pool_.clear();
	pool_.reserve(capacity);
	for (size_t i = 0; i < capacity; ++i) {
		auto p = std::make_unique<AlmaParticle>();
		p->wt.Initialize();
		p->color.Initialize();
		pool_.push_back(std::move(p));
	}
	cursor_ = 0;
}

void AlmaBurstParticles::Emit(const AlmaEmitParams& params) {
	if (pool_.empty()) {
		return;
	}
	std::uniform_real_distribution<float> angle(0.0f, 2.0f * std::numbers::pi_v<float>);
	std::uniform_real_distribution<float> elev(-0.6f, 1.2f);
	std::uniform_real_distribution<float> speed(params.speedMin, params.speedMax);
	std::uniform_real_distribution<float> unit(-1.0f, 1.0f);
	std::uniform_real_distribution<float> lifeJitter(0.75f, 1.25f);

	for (int n = 0; n < params.count; ++n) {
		// 空きを探す（見つからなければ一番古いものを上書き）
		AlmaParticle* target = nullptr;
		for (size_t k = 0; k < pool_.size(); ++k) {
			size_t idx = (cursor_ + k) % pool_.size();
			if (!pool_[idx]->active) {
				target = pool_[idx].get();
				cursor_ = (idx + 1) % pool_.size();
				break;
			}
		}
		if (!target) {
			target = pool_[cursor_].get();
			cursor_ = (cursor_ + 1) % pool_.size();
		}

		float a = angle(rng_);
		float e = elev(rng_);
		float s = speed(rng_);
		target->velocity = {std::cos(a) * std::cos(e) * s + params.bias.x, std::sin(e) * s + params.bias.y, std::sin(a) * std::cos(e) * s * 0.5f + params.bias.z};
		target->wt.translation_ = params.position;
		target->wt.rotation_ = {unit(rng_) * 3.0f, unit(rng_) * 3.0f, unit(rng_) * 3.0f};
		target->spinAxis = {unit(rng_) * params.spin, unit(rng_) * params.spin, unit(rng_) * params.spin};
		target->startScale = params.scale * (0.7f + 0.3f * (unit(rng_) + 1.0f));
		target->wt.scale_ = {target->startScale, target->startScale, target->startScale};
		target->baseColor = params.rainbow ? FxUtil::Rainbow((unit(rng_) + 1.0f) * 0.5f, params.color.w) : params.color;
		target->maxLife = params.life * lifeJitter(rng_);
		target->life = target->maxLife;
		target->gravity = params.gravity;
		target->texture = params.texture;
		target->alt = params.useAltModel;
		target->active = true;
	}
}

void AlmaBurstParticles::Update() {
	const float dt = 1.0f / 60.0f;
	for (auto& p : pool_) {
		if (!p->active) {
			continue;
		}
		p->life -= dt;
		if (p->life <= 0.0f) {
			p->active = false;
			continue;
		}
		float t = p->life / p->maxLife; // 1 → 0
		p->velocity.y -= p->gravity;
		p->velocity.x *= 0.97f;
		p->velocity.z *= 0.97f;
		p->wt.translation_ += p->velocity;
		p->wt.rotation_ += p->spinAxis;
		float s = p->startScale * (0.35f + 0.65f * t);
		p->wt.scale_ = {s, s, s};
		AlmaVector4 c = p->baseColor;
		c.w *= std::clamp(t * 1.6f, 0.0f, 1.0f);
		p->color.SetColor(c);
		AlmaWorldTransformUpdate(p->wt);
	}
}

void AlmaBurstParticles::Draw() {
	if (!camera_) {
		return;
	}
	for (auto& p : pool_) {
		if (!p->active) {
			continue;
		}
		AlmaModel* m = p->alt ? altModel_ : model_;
		if (!m) {
			continue;
		}
		if (p->texture != 0) {
			m->Draw(p->wt, *camera_, p->texture, &p->color);
		} else {
			m->Draw(p->wt, *camera_, &p->color);
		}
	}
}

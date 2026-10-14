#include "BaseBeamState.h"
#include "FearWormEnemy.h"
#include "App/Game/Player/Player.h"
#include "Engine/Effect/EffectManager.h"
#include <algorithm>
#include <array>

void BaseBeamState::Enter(FearWormEnemy& boss) const { boss.beamTimer_ = 0.0f; }

class BeamWaitState final : public BaseBeamState {
public:
    FearWormBeamState GetType() const override { return FearWormBeamState::Wait; }
    void Update(FearWormEnemy& boss, float) const override {
        boss.beamCurrentLength_ = 0.0f;
        boss.beamCurrentWidth_ = 0.0f;
        boss.Attack();
        if (boss.beamTimer_ >= 4.0f) boss.ChangeBeamState(FearWormBeamState::Charge);
    }
};

class BeamChargeState final : public BaseBeamState {
public:
    FearWormBeamState GetType() const override { return FearWormBeamState::Charge; }
    void Enter(FearWormEnemy& boss) const override {
        BaseBeamState::Enter(boss);
        if (boss.isHeadChargeActive_) boss.FinishHeadChargeAttack();
        boss.beamEffectTimer_ = 0.0f;
        if (boss.player_ && !boss.segments_.empty()) {
            boss.headAimDirection_ = NormalizeSafe(boss.player_->GetTranslate() - boss.segments_[0].position);
            EffectManager::GetInstance()->PlayEffect("WormShotFlash", boss.beamMuzzlePosition_);
        }
    }
    void Update(FearWormEnemy& boss, float dt) const override {
        boss.beamCurrentLength_ = 0.0f;
        boss.beamCurrentWidth_ = 0.0f;
        const Vector3 direction = NormalizeSafe(boss.player_->GetTranslate() - boss.segments_[0].position);
        boss.headAimDirection_ = NormalizeSafe(Lerp(boss.headAimDirection_, direction, 0.08f));
        boss.beamEffectTimer_ += dt;
        if (boss.beamEffectTimer_ >= 0.25f) {
            boss.beamEffectTimer_ = 0.0f;
            EffectManager::GetInstance()->PlayEffect("WormShotFlash", boss.beamMuzzlePosition_);
        }
        if (boss.beamTimer_ >= 1.5f) boss.ChangeBeamState(FearWormBeamState::Fire);
    }
};

class BeamFireState final : public BaseBeamState {
public:
    FearWormBeamState GetType() const override { return FearWormBeamState::Fire; }
    void Enter(FearWormEnemy& boss) const override {
        BaseBeamState::Enter(boss);
        boss.beamEffectTimer_ = 0.0f;
        if (boss.player_ && !boss.segments_.empty()) {
            EffectManager::GetInstance()->PlayEffect("WormShotFlash", boss.beamMuzzlePosition_);
        }
    }
    void Update(FearWormEnemy& boss, float dt) const override {
        boss.beamCurrentLength_ = (std::min)(400.0f, boss.beamCurrentLength_ + 1000.0f * dt);
        boss.beamCurrentWidth_ = 3.5f;
        const Vector3 direction = NormalizeSafe(boss.player_->GetTranslate() - boss.segments_[0].position);
        boss.headAimDirection_ = NormalizeSafe(Lerp(boss.headAimDirection_, direction, 0.015f));
        boss.CheckBeamCollision();
        boss.beamEffectTimer_ += dt;
        if (boss.beamEffectTimer_ >= 0.2f) {
            boss.beamEffectTimer_ = 0.0f;
            EffectManager::GetInstance()->PlayEffect("WormShotFlash", boss.beamMuzzlePosition_);
        }
        if (boss.beamTimer_ >= 3.0f) boss.ChangeBeamState(FearWormBeamState::FadeOut);
    }
};

class BeamFadeOutState final : public BaseBeamState {
public:
    FearWormBeamState GetType() const override { return FearWormBeamState::FadeOut; }
    void Update(FearWormEnemy& boss, float dt) const override {
        boss.beamCurrentWidth_ = (std::max)(0.0f, boss.beamCurrentWidth_ - 8.0f * dt);
        const Vector3 direction = NormalizeSafe(boss.player_->GetTranslate() - boss.segments_[0].position);
        boss.headAimDirection_ = NormalizeSafe(Lerp(boss.headAimDirection_, direction, 0.01f));
        if (boss.beamTimer_ >= 0.5f) boss.ChangeBeamState(FearWormBeamState::Wait);
    }
};

const BaseBeamState& GetBeamState(FearWormBeamState type)
{
    static const BeamWaitState wait;
    static const BeamChargeState charge;
    static const BeamFireState fire;
    static const BeamFadeOutState fade;
    static const std::array<const BaseBeamState*, 4> states { &wait, &charge, &fire, &fade };
    const auto index = static_cast<size_t>(type);
    return *states[index < states.size() ? index : 0];
}

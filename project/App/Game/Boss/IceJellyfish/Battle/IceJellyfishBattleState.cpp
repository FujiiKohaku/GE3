#include "IceJellyfishBattleState.h"
#include "../IceJellyfish.h"
#include "App/Game/Player/Player.h"
#include "Engine/Time/TimeManager.h"
#include <cmath>

void IceJellyfishBattleState::Reset()
{
    nextAttackIndex_ = 0;
    isPulseAttackArmed_ = false;
}

void IceJellyfishBattleState::Update(IceJellyfish& boss)
{
    if (boss.player_ != nullptr) {
        const Vector3 playerPosition = boss.player_->GetTranslate();
        boss.bodyPosition_ = {
            boss.hoverCenterX_ + std::sin(boss.animationTime_ * 0.48f) * 8.0f,
            boss.hoverCenterY_ + std::sin(boss.animationTime_ * 1.10f) * 5.0f,
            playerPosition.z + boss.kHoverDistance
        };
    } else {
        boss.bodyPosition_ = boss.basePosition_;
    }
    boss.basePosition_ = boss.bodyPosition_;
    boss.transform_.translate = boss.bodyPosition_;

    boss.coreExposed_ = boss.AreAllTentaclesDestroyed();
    UpdateAttackSequence(boss, boss.animationTime_ * 1.10f, TimeManager::GetInstance()->GetDeltaTime());
    boss.coreExposed_ = boss.AreAllTentaclesDestroyed();
}

void IceJellyfishBattleState::UpdateAttackSequence(IceJellyfish& boss, float floatPhase, float deltaTimeSeconds)
{
    // 前の攻撃が継続中でも更新し、新たな攻撃の開始と両立させる。
    boss.UpdatePulseBarrageAttack(deltaTimeSeconds);
    boss.UpdateIceSpearAttack(deltaTimeSeconds);
    boss.UpdateIcePillarAttack(deltaTimeSeconds);
    const float pulse = std::sin(floatPhase);
    if (pulse <= -0.72f) {
        isPulseAttackArmed_ = true;
    }
    if (!isPulseAttackArmed_ || pulse < -0.05f) {
        return;
    }

    switch (nextAttackIndex_) {
    case 0: boss.StartPulseBarrageAttack(); break;
    case 1: boss.StartIceSpearAttack(); break;
    case 2: boss.StartIcePillarAttack(); break;
    }
    nextAttackIndex_ = (nextAttackIndex_ + 1) % 3;
    isPulseAttackArmed_ = false;
}

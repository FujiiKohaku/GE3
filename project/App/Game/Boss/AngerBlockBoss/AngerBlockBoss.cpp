#include "AngerBlockBoss.h"

#include "App/Game/Player/Player.h"
#include "App/Game/Enemy/Bullet/NormalEnemyBullet.h"
#include "Engine/3D/Object3d.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/math/MathStruct.h"
#include "Engine/Time/TimeManager.h"
#include <algorithm>
#include <cmath>

namespace {
std::unique_ptr<Object3d> CreatePart(Model* model, const Vector3& scale, const Vector4& color)
{
    auto part = std::make_unique<Object3d>();
    part->Initialize(Object3dManager::GetInstance());
    part->SetModel(model);
    part->SetScale(scale);
    part->SetColor(color);
    part->SetEnableLighting(true);
    return part;
}

float EaseInOut(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float EaseInCubic(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * t;
}

float EaseOutCubic(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    float inverse = 1.0f - t;
    return 1.0f - inverse * inverse * inverse;
}
}

void AngerBlockBoss::Initialize(Model* model, Model* bulletModel, Player* player)
{
    player_ = player;
    bulletModel_ = bulletModel;
    body_ = CreatePart(model, { 14.0f, 11.0f, 8.0f }, { 0.16f, 0.12f, 0.10f, 1.0f });
    core_ = CreatePart(model, { 4.5f, 3.0f, 1.5f }, { 1.0f, 0.08f, 0.02f, 1.0f });
    leftHand_ = CreatePart(model, { 7.0f, 7.0f, 7.0f }, { 0.22f, 0.13f, 0.10f, 1.0f });
    rightHand_ = CreatePart(model, { 7.0f, 7.0f, 7.0f }, { 0.22f, 0.13f, 0.10f, 1.0f });
    hp_ = coreHp_;
    UpdatePartTransforms();
}

void AngerBlockBoss::SetPosition(const Vector3& position)
{
    bodyPosition_ = position;
    transform_.translate = position;
    UpdatePartTransforms();
}

void AngerBlockBoss::Update()
{
    const float deltaTime = TimeManager::GetInstance()->GetDeltaTime();
    if (isDead_) {
        deathTimer_ += deltaTime;
        bodyPosition_.y -= 8.0f * deltaTime;
        bodyPosition_.z += 4.0f * deltaTime;
        UpdatePartTransforms();
        return;
    }

    battleTime_ += deltaTime;
    isMadMode_ = coreHp_ <= 50.0f;

    if (player_ != nullptr) {
        const Vector3 playerPosition = player_->GetTranslate();
        bodyPosition_.x = playerPosition.x + std::sin(battleTime_ * 0.8f) * 7.0f;
        bodyPosition_.y = playerPosition.y + 5.0f + std::sin(battleTime_ * 1.7f) * 1.5f;
        bodyPosition_.z = playerPosition.z + 90.0f;
    }

    const float cycleLength = isMadMode_ ? 12.0f : 13.5f;
    const float cycle = std::fmod(battleTime_, cycleLength);
    if (cycle < previousCycle_) {
        previousCycle_ = 0.0f;
        doublePunchTargetCaptured_ = false;
    }
    Vector3 leftHome = bodyPosition_ + Vector3 { -18.0f, 0.0f, -2.0f };
    Vector3 rightHome = bodyPosition_ + Vector3 { 18.0f, 0.0f, -2.0f };
    leftHandPosition_ = leftHome;
    rightHandPosition_ = rightHome;
    leftWarning_ = false;
    rightWarning_ = false;

    const bool handsDestroyed = leftHandHp_ <= 0.0f && rightHandHp_ <= 0.0f;
    if (handsDestroyed) {
        UpdateCoreBehavior(deltaTime);
        previousCycle_ = cycle;
        UpdatePartTransforms();
        return;
    }
    coreRushWarning_ = false;

    if (player_ != nullptr && cycle >= 0.8f && cycle < 2.1f && leftHandHp_ > 0.0f) {
        leftWarning_ = true;
        float windup = EaseInOut((cycle - 0.8f) / 1.3f);
        float shake = std::sin(cycle * 70.0f) * (0.3f + windup * 0.8f);
        leftHandPosition_ = leftHome + Vector3 { shake, -shake * 0.4f, 12.0f * windup };
    } else if (player_ != nullptr && cycle >= 2.1f && cycle < 2.5f && leftHandHp_ > 0.0f) {
        if (previousCycle_ < 2.1f) leftPunchTarget_ = player_->GetTranslate();
        float reach = EaseInCubic((cycle - 2.1f) / 0.4f);
        Vector3 attackStart = leftHome + Vector3 { 0.0f, 0.0f, 12.0f };
        leftHandPosition_ = attackStart + (leftPunchTarget_ - attackStart) * reach;
        CheckHandHit(leftHandPosition_);
    } else if (cycle >= 2.5f && cycle < 3.2f && leftHandHp_ > 0.0f) {
        float returnRate = EaseOutCubic((cycle - 2.5f) / 0.7f);
        leftHandPosition_ = leftPunchTarget_ + (leftHome - leftPunchTarget_) * returnRate;
    }

    if (player_ != nullptr && cycle >= 3.8f && cycle < 5.1f && rightHandHp_ > 0.0f) {
        rightWarning_ = true;
        float windup = EaseInOut((cycle - 3.8f) / 1.3f);
        float shake = std::sin(cycle * 70.0f) * (0.3f + windup * 0.8f);
        rightHandPosition_ = rightHome + Vector3 { shake, shake * 0.4f, 12.0f * windup };
    } else if (player_ != nullptr && cycle >= 5.1f && cycle < 5.5f && rightHandHp_ > 0.0f) {
        if (previousCycle_ < 5.1f) rightPunchTarget_ = player_->GetTranslate();
        float reach = EaseInCubic((cycle - 5.1f) / 0.4f);
        Vector3 attackStart = rightHome + Vector3 { 0.0f, 0.0f, 12.0f };
        rightHandPosition_ = attackStart + (rightPunchTarget_ - attackStart) * reach;
        CheckHandHit(rightHandPosition_);
    } else if (cycle >= 5.5f && cycle < 6.2f && rightHandHp_ > 0.0f) {
        float returnRate = EaseOutCubic((cycle - 5.5f) / 0.7f);
        rightHandPosition_ = rightPunchTarget_ + (rightHome - rightPunchTarget_) * returnRate;
    }

    if (cycle >= 7.0f) {
        float squeezePhase = (cycle - 7.0f) / 2.0f;
        if (squeezePhase <= 1.0f) {
        float squeeze = squeezePhase < 0.5f
            ? EaseInOut(squeezePhase * 2.0f)
            : EaseInOut((1.0f - squeezePhase) * 2.0f);
        if (leftHandHp_ > 0.0f) leftHandPosition_.x += 10.0f * squeeze;
        if (rightHandHp_ > 0.0f) rightHandPosition_.x -= 10.0f * squeeze;
        }
    }

    if (player_ != nullptr && cycle >= 9.4f && cycle < 10.7f) {
        leftWarning_ = leftHandHp_ > 0.0f;
        rightWarning_ = rightHandHp_ > 0.0f;
        float windup = EaseInOut((cycle - 9.4f) / 1.3f);
        float shake = std::sin(cycle * 75.0f) * (0.4f + windup);
        if (leftHandHp_ > 0.0f) leftHandPosition_ = leftHome + Vector3 { shake, 0.0f, 12.0f * windup };
        if (rightHandHp_ > 0.0f) rightHandPosition_ = rightHome + Vector3 { -shake, 0.0f, 12.0f * windup };
    } else if (player_ != nullptr && cycle >= 10.7f && cycle < 11.1f) {
        if (!doublePunchTargetCaptured_) {
            doublePunchTarget_ = player_->GetTranslate();
            doublePunchTargetCaptured_ = true;
        }
        float reach = EaseInCubic((cycle - 10.7f) / 0.4f);
        if (leftHandHp_ > 0.0f) {
            Vector3 target = doublePunchTarget_ + Vector3 { -5.0f, 0.0f, 0.0f };
            leftHandPosition_ = leftHome + (target - leftHome) * reach;
            CheckHandHit(leftHandPosition_);
        }
        if (rightHandHp_ > 0.0f) {
            Vector3 target = doublePunchTarget_ + Vector3 { 5.0f, 0.0f, 0.0f };
            rightHandPosition_ = rightHome + (target - rightHome) * reach;
            CheckHandHit(rightHandPosition_);
        }
    } else if (cycle >= 11.1f && cycle < 11.8f && doublePunchTargetCaptured_) {
        float returnRate = EaseOutCubic((cycle - 11.1f) / 0.7f);
        if (leftHandHp_ > 0.0f) {
            Vector3 target = doublePunchTarget_ + Vector3 { -5.0f, 0.0f, 0.0f };
            leftHandPosition_ = target + (leftHome - target) * returnRate;
        }
        if (rightHandHp_ > 0.0f) {
            Vector3 target = doublePunchTarget_ + Vector3 { 5.0f, 0.0f, 0.0f };
            rightHandPosition_ = target + (rightHome - target) * returnRate;
        }
    }

    previousCycle_ = cycle;
    UpdatePartTransforms();
}

void AngerBlockBoss::UpdateCoreBehavior(float deltaTime)
{
    constexpr float kPatternDuration = 4.8f;
    coreRushTimer_ += deltaTime;
    if (coreRushTimer_ >= kPatternDuration) {
        coreRushTimer_ = 0.0f;
        corePattern_ = (corePattern_ + 1) % 3;
        coreRushTargetCaptured_ = false;
        coreShotWave_ = 0;
    }

    if (corePattern_ == 0) UpdateCoreRush();
    if (corePattern_ == 1) UpdateCoreBurst();
    if (corePattern_ == 2) UpdateCoreStrafe();

}

void AngerBlockBoss::UpdateCoreRush()
{
    if (player_ == nullptr) return;

    constexpr float kChargeEnd = 1.5f;
    constexpr float kRushEnd = 2.0f;
    constexpr float kReturnEnd = 3.1f;

    Vector3 home = player_->GetTranslate() + Vector3 { 0.0f, 5.0f, 90.0f };
    coreRushWarning_ = coreRushTimer_ < kChargeEnd;
    if (coreRushTimer_ < kChargeEnd) {
        float charge = EaseInOut(coreRushTimer_ / kChargeEnd);
        float shake = std::sin(coreRushTimer_ * 75.0f) * (0.25f + charge * 1.4f);
        bodyPosition_ = home + Vector3 { shake, -shake * 0.45f, 15.0f * charge };
        return;
    }

    if (!coreRushTargetCaptured_) {
        coreRushStart_ = home + Vector3 { 0.0f, 0.0f, 15.0f };
        coreRushTarget_ = player_->GetTranslate();
        coreRushTargetCaptured_ = true;
    }

    if (coreRushTimer_ < kRushEnd) {
        float rush = EaseInCubic((coreRushTimer_ - kChargeEnd) / (kRushEnd - kChargeEnd));
        bodyPosition_ = coreRushStart_ + (coreRushTarget_ - coreRushStart_) * rush;
        Vector3 difference = player_->GetTranslate() - bodyPosition_;
        if (Dot(difference, difference) <= 100.0f) player_->ApplyDamage(3);
        return;
    }

    if (coreRushTimer_ < kReturnEnd) {
        float returnRate = EaseOutCubic((coreRushTimer_ - kRushEnd) / (kReturnEnd - kRushEnd));
        bodyPosition_ = coreRushTarget_ + (home - coreRushTarget_) * returnRate;
        return;
    }

    bodyPosition_ = home;
}

void AngerBlockBoss::UpdateCoreBurst()
{
    if (player_ == nullptr) return;
    Vector3 home = player_->GetTranslate() + Vector3 { 0.0f, 5.0f, 90.0f };
    bodyPosition_ = home;

    const float nextShotTime = 1.2f + static_cast<float>(coreShotWave_) * 0.9f;
    coreRushWarning_ = coreShotWave_ < 3 &&
        coreRushTimer_ >= nextShotTime - 0.55f && coreRushTimer_ < nextShotTime;
    if (coreShotWave_ < 3 && coreRushTimer_ >= nextShotTime) {
        FireCoreBurst(true);
        coreShotWave_ += 1;
    }
}

void AngerBlockBoss::UpdateCoreStrafe()
{
    if (player_ == nullptr) return;
    constexpr float kTwoPi = 6.28318530718f;
    float progress = coreRushTimer_ / 4.8f;
    Vector3 home = player_->GetTranslate() + Vector3 { 0.0f, 5.0f, 90.0f };
    bodyPosition_ = home + Vector3 {
        std::sin(progress * kTwoPi) * 28.0f,
        std::sin(progress * kTwoPi * 2.0f) * 7.0f,
        0.0f };

    const float nextShotTime = 0.8f + static_cast<float>(coreShotWave_) * 1.2f;
    coreRushWarning_ = coreShotWave_ < 3 &&
        coreRushTimer_ >= nextShotTime - 0.35f && coreRushTimer_ < nextShotTime;
    if (coreShotWave_ < 3 && coreRushTimer_ >= nextShotTime) {
        FireCoreBurst(false);
        coreShotWave_ += 1;
    }
}

void AngerBlockBoss::FireCoreBurst(bool spread)
{
    if (player_ == nullptr || bulletModel_ == nullptr) return;
    const Vector3 muzzle = bodyPosition_ + Vector3 { 0.0f, 0.0f, -10.0f };
    const Vector3 targetDirection = Normalize(player_->GetTranslate() - muzzle);
    const int32_t minimumIndex = spread ? -1 : 0;
    const int32_t maximumIndex = spread ? 1 : 0;
    for (int32_t index = minimumIndex; index <= maximumIndex; ++index) {
        auto bullet = std::make_unique<NormalEnemyBullet>();
        bullet->Initialize(bulletModel_);
        Vector3 direction = Normalize(
            targetDirection + Vector3 { static_cast<float>(index) * 0.16f, 0.0f, 0.0f });
        bullet->SetTranslate(muzzle);
        bullet->SetVelocity(direction * 0.85f);
        bullet->SetColor({ 1.0f, 0.12f, 0.02f, 1.0f });
        bullet->SetDamage(1);
        AddEnemyBullet(std::move(bullet));
    }
}

void AngerBlockBoss::CheckHandHit(const Vector3& handPosition)
{
    if (player_ == nullptr) return;
    Vector3 difference = player_->GetTranslate() - handPosition;
    if (Dot(difference, difference) <= 36.0f) {
        player_->ApplyDamage(2);
    }
}

void AngerBlockBoss::UpdatePartTransforms()
{
    if (body_) {
        body_->SetTranslate(bodyPosition_);
        body_->SetRotate({ 0.0f, 0.0f, std::sin(battleTime_ * 9.0f) * (isMadMode_ ? 0.04f : 0.015f) });
        body_->Update();
    }
    if (core_) {
        core_->SetTranslate(bodyPosition_ + Vector3 { 0.0f, 0.0f, -8.5f });
        float pulseAmount = coreRushWarning_ ? 0.28f : 0.12f;
        float pulseSpeed = coreRushWarning_ ? 16.0f : (isMadMode_ ? 10.0f : 5.0f);
        float pulse = 1.0f + std::sin(battleTime_ * pulseSpeed) * pulseAmount;
        core_->SetColor(coreRushWarning_
            ? Vector4 { 1.0f, 0.45f, 0.02f, 1.0f }
            : Vector4 { 1.0f, 0.08f, 0.02f, 1.0f });
        core_->SetScale({ 4.5f * pulse, 3.0f * pulse, 1.5f });
        core_->Update();
    }
    if (leftHand_ && leftHandHp_ > 0.0f) {
        leftHand_->SetColor(leftWarning_
            ? Vector4 { 1.0f, 0.18f, 0.04f, 1.0f }
            : Vector4 { 0.22f, 0.13f, 0.10f, 1.0f });
        leftHand_->SetTranslate(leftHandPosition_);
        leftHand_->Update();
    }
    if (rightHand_ && rightHandHp_ > 0.0f) {
        rightHand_->SetColor(rightWarning_
            ? Vector4 { 1.0f, 0.18f, 0.04f, 1.0f }
            : Vector4 { 0.22f, 0.13f, 0.10f, 1.0f });
        rightHand_->SetTranslate(rightHandPosition_);
        rightHand_->Update();
    }
}

void AngerBlockBoss::Draw()
{
    if (body_) body_->Draw();
    if (core_) core_->Draw();
    if (leftHand_ && leftHandHp_ > 0.0f) leftHand_->Draw();
    if (rightHand_ && rightHandHp_ > 0.0f) rightHand_->Draw();
}

void AngerBlockBoss::GetCollisionParts(std::vector<EnemyCollisionPart>& parts) const
{
    parts.clear();
    parts.push_back({ bodyPosition_ + Vector3 { 0.0f, 0.0f, -8.5f }, 5.0f, 0 });
    if (leftHandHp_ > 0.0f) parts.push_back({ leftHandPosition_, 5.5f, 1 });
    if (rightHandHp_ > 0.0f) parts.push_back({ rightHandPosition_, 5.5f, 2 });
    parts.push_back({ bodyPosition_, 10.0f, 3 });
}

bool AngerBlockBoss::IsCollisionPartDamageable(int32_t partIndex) const
{
    if (partIndex == 0) return leftHandHp_ <= 0.0f && rightHandHp_ <= 0.0f;
    return partIndex == 1 || partIndex == 2;
}

void AngerBlockBoss::ApplyDamageToPart(int32_t partIndex, float damage)
{
    if (partIndex == 1) leftHandHp_ = (std::max)(0.0f, leftHandHp_ - damage);
    if (partIndex == 2) rightHandHp_ = (std::max)(0.0f, rightHandHp_ - damage);
    if (partIndex == 0 && IsCollisionPartDamageable(0)) {
        coreHp_ = (std::max)(0.0f, coreHp_ - damage);
        hp_ = coreHp_;
        if (coreHp_ <= 0.0f) SetDead(true);
    }
}

float AngerBlockBoss::GetHeadHpFraction() const
{
    return coreHp_ / 100.0f;
}

float AngerBlockBoss::GetBodyHpFraction() const
{
    return (leftHandHp_ + rightHandHp_) / 50.0f;
}

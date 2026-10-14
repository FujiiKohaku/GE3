#include "FearWormEnemy.h"
#include "Engine/3D/Object3d.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/Effect/EffectManager.h"
#include "Engine/Time/TimeManager.h"
#include <algorithm>
#include <cmath>
#include <utility>

namespace {
constexpr int32_t kWormSegmentCount = 10;
constexpr float kHeadHp = 20.0f;
constexpr float kBodyHp = 6.0f;
constexpr float kBodyScale = 2.45f;
constexpr float kHeadScale = 3.45f;
constexpr float kTailScale = 2.15f;
constexpr float kCollisionRadiusScale = 0.90f;
constexpr Vector4 kHeadColor = { 0.30f, 0.88f, 1.0f, 1.0f };
constexpr Vector4 kVulnerableHeadColor = { 1.0f, 0.18f, 0.04f, 1.0f };
constexpr Vector4 kChargingHeadColor = { 1.0f, 0.48f, 0.08f, 1.0f };
constexpr Vector4 kFiringHeadColor = { 1.0f, 0.08f, 0.02f, 1.0f };
constexpr Vector4 kBodyColor = { 0.78f, 0.20f, 1.0f, 1.0f };
constexpr Vector4 kHitFlashColor = { 1.0f, 0.95f, 0.35f, 1.0f };
constexpr float kBodyPulseFrequency = 5.5f;
constexpr float kBodyPulseAmplitude = 0.08f;
constexpr float kChargePulseAdd = 0.12f;
constexpr float kChargePulseFrequency = 24.0f;
constexpr float kChargePulseAmplitude = 0.08f;
constexpr float kSegmentRotationOffset = 0.35f;
constexpr float kDamageHitFlashDuration = 0.10f;
constexpr float kGuardHitFlashDuration = 0.08f;
constexpr float kMinimumHealth = 0.0001f;
}

void FearWormEnemy::InitializeSegments(Model* model)
{
    // 体のセグメントを初期化
    segments_.clear();
    segments_.reserve(kWormSegmentCount);

    // 体作成
    for (int32_t index = 0; index < kWormSegmentCount; index = index + 1) {
        Segment segment { };
        segment.object = std::make_unique<Object3d>();
        segment.object->Initialize(Object3dManager::GetInstance());
        segment.object->SetMaterial("resources/Shaders/Object3D/ShadowStandard");
        segment.object->SetCastShadow(true);
        segment.object->SetReceiveShadow(true);
        segment.object->SetModel(model);
        segment.object->SetEnableLighting(false);
        segment.isHead = false;
        segment.isAlive = true;
        segment.hp = kBodyHp;

        float scale = kBodyScale;
        if (index == 0) { // 頭のセグメント
            segment.isHead = true;
            segment.hp = kHeadHp;
            scale = kHeadScale;
        }

        if (index == kWormSegmentCount - 1) { // 尾のセグメント
            scale = kTailScale;
        }

        segment.scale = { scale, scale, scale };
        segment.radius = scale * kCollisionRadiusScale;

        segments_.push_back(std::move(segment));
    }
}

void FearWormEnemy::UpdateSegmentObjects()
{
    for (size_t index = 0; index < segments_.size(); index = index + 1) {
        Segment& segment = segments_[index];

        if (segment.object == nullptr) {
            continue;
        }

        if (segment.hitFlashTimer > 0.0f) {
            segment.hitFlashTimer -= TimeManager::GetInstance()->GetDeltaTime();
            if (segment.hitFlashTimer < 0.0f) {
                segment.hitFlashTimer = 0.0f;
            }
        }

        Vector4 color { };
        if (segment.isHead) {
            color = kHeadColor;

            if (!HasAliveBodyParts()) {
                color = kVulnerableHeadColor;
            }

            if (isHeadChargeActive_) {
                color = kChargingHeadColor;
            }

            if (isHeadChargeFiring_) {
                color = kFiringHeadColor;
            }
        } else {
            color = kBodyColor;
        }

        if (segment.hitFlashTimer > 0.0f) {
            color = kHitFlashColor;
        }

        float pulse = 1.0f + std::sin(moveTime_ * kBodyPulseFrequency + static_cast<float>(index)) * kBodyPulseAmplitude;
        if (segment.isHead && isHeadChargeActive_) {
            pulse += kChargePulseAdd;
            pulse += std::sin(headChargeTimer_ * kChargePulseFrequency) * kChargePulseAmplitude;
        }

        Vector3 scale { };
        scale.x = segment.scale.x * pulse;
        scale.y = segment.scale.y * pulse;
        scale.z = segment.scale.z * pulse;

        Vector3 rotate { };
        if (segment.isHead && GetBossState() == BossState::Dead) {
            // 死亡中は死亡状態の更新で回転と座標を設定しUpdateも呼ぶため、共通の上書き処理をバイパスする
            segment.object->SetScale(scale);
            segment.object->SetColor(color);
            continue;
        }

        rotate.y = moveTime_ + static_cast<float>(index) * kSegmentRotationOffset;
        bool isBeamAiming = !HasAliveBodyParts() && (beamState_ == BeamState::Charge || beamState_ == BeamState::Fire || beamState_ == BeamState::FadeOut);
        if (segment.isHead && (isHeadChargeActive_ || isBeamAiming)) {
            rotate = HeadLookRotation();
        }

        segment.object->SetScale(scale);
        segment.object->SetRotate(rotate);
        segment.object->SetTranslate(segment.position);
        segment.object->SetColor(color);
        segment.object->Update();
    }
}

void FearWormEnemy::Draw()
{
    if (GetBossState() != BossState::Wait) {
        for (Segment& segment : segments_) {
            if (!segment.isAlive) {
                continue;
            }

            if (segment.object == nullptr) {
                continue;
            }

            segment.object->Draw();
        }
    }

    DrawBeam();
}

void FearWormEnemy::DrawShadow(ShadowMapRenderer& renderer)
{
    if (GetBossState() == BossState::Wait) { return; }
    for (Segment& segment : segments_) {
        if (segment.isAlive && segment.object != nullptr) { segment.object->DrawShadow(renderer); }
    }
}

void FearWormEnemy::SubmitRaytracing(DxrRenderer& renderer) {
    if (GetBossState() == BossState::Wait) { return; }
    for (const Segment& segment : segments_) {
        if (segment.isAlive && segment.object) { segment.object->SubmitRaytracing(renderer); }
    }
    if (!segments_.empty() && segments_[0].isAlive && beamPlane_
        && (beamState_ == BeamState::Charge || beamState_ == BeamState::Fire || beamState_ == BeamState::FadeOut)) {
        beamPlane_->SubmitRaytracing(renderer);
    }
}

void FearWormEnemy::GetCollisionParts(std::vector<EnemyCollisionPart>& parts) const
{
    if (GetBossState() == BossState::Wait) {
        return;
    }

    // 生存中の部位だけを当たり判定として登録する。
    for (size_t index = 0; index < segments_.size(); index = index + 1) {
        const Segment& segment = segments_[index];

        if (!segment.isAlive) {
            continue;
        }

        EnemyCollisionPart part { };
        part.position = segment.position;
        part.radius = segment.radius;
        part.partIndex = static_cast<int32_t>(index);

        parts.push_back(part);
    }
}

bool FearWormEnemy::IsCollisionPartDamageable(int32_t partIndex) const
{
    // 範囲外または破壊済みの部位にはダメージを通さない。
    if (!IsValidSegmentIndex(partIndex)) {
        return false;
    }

    const Segment& segment = segments_[partIndex];
    if (!segment.isAlive) {
        return false;
    }

    // 胴体が残っている間は頭部を無敵にする。
    if (segment.isHead && HasAliveBodyParts()) {
        return false;
    }

    return true;
}

void FearWormEnemy::ApplyDamageToPart(int32_t partIndex, float damage)
{
    if (IsDead() || !std::isfinite(damage) || damage <= 0.0f) { return; }
    if (!IsValidSegmentIndex(partIndex)) {
        return;
    }

    // ダメージ無効の部位ならガード演出だけを出す。
    if (!IsCollisionPartDamageable(partIndex)) {
        OnCollisionPartGuarded(partIndex, segments_[partIndex].position);
        return;
    }

    // HPを減らし、まだ残っていれば被弾表示だけ更新する。
    Segment& segment = segments_[partIndex];
    segment.hp -= damage;
    segment.hitFlashTimer = kDamageHitFlashDuration;

    if (segment.hp > 0.0f) {
        return;
    }

    // 頭部のHPが尽きた場合はボス撃破へ移行する。
    if (segment.isHead) {
        SetDead(true);
        return;
    }

    // 胴体の場合は対象部位だけを破壊する。
    segment.isAlive = false;
    PlayBodyBreakEffect(segment.position);

    // 最後の胴体が壊れた瞬間に頭部の弱点化を通知する。
    if (!vulnerableEffectPlayed_ && !HasAliveBodyParts()) {
        vulnerableEffectPlayed_ = true;
        PlayHeadVulnerableEffect(GetPosition());
    }
}

void FearWormEnemy::OnCollisionPartGuarded(int32_t partIndex, const Vector3& position)
{
    if (!IsValidSegmentIndex(partIndex)) {
        return;
    }

    // 対象部位を短時間点滅させ、命中位置にガード演出を出す。
    segments_[partIndex].hitFlashTimer = kGuardHitFlashDuration;
    PlayHeadGuardEffect(position);
}

void FearWormEnemy::PlayBodyBreakEffect(const Vector3& position)
{
    EffectManager::GetInstance()->PlayEffect("WormBodyBreak", position);
}

void FearWormEnemy::PlayHeadGuardEffect(const Vector3& position)
{
    EffectManager::GetInstance()->PlayEffect("WormHeadGuard", position);
}

void FearWormEnemy::PlayHeadVulnerableEffect(const Vector3& position)
{
    EffectManager::GetInstance()->PlayEffect("WormHeadVulnerable", position);
}

float FearWormEnemy::HealthRate() const
{
    float currentHealth = 0.0f;
    float maxHealth = 0.0f;

    // 全部位の最大HPと、生存部位の残りHPを合計する。
    for (const Segment& segment : segments_) {
        if (segment.isHead) {
            maxHealth += kHeadHp;
        } else {
            maxHealth += kBodyHp;
        }

        if (!segment.isAlive) {
            continue;
        }

        if (segment.hp > 0.0f) {
            currentHealth += segment.hp;
        }
    }

    if (maxHealth <= kMinimumHealth) {
        return 0.0f;
    }

    // 最大HPに対する割合を求め、0～1の範囲に収める。
    float healthRate = currentHealth / maxHealth;

    if (healthRate < 0.0f) {
        healthRate = 0.0f;
    }

    if (healthRate > 1.0f) {
        healthRate = 1.0f;
    }

    return healthRate;
}

bool FearWormEnemy::HasAliveBodyParts() const
{
    // 先頭の頭部を除き、生存している胴体を探す。
    for (size_t index = 1; index < segments_.size(); index = index + 1) {
        if (segments_[index].isAlive) {
            return true;
        }
    }

    return false;
}

bool FearWormEnemy::IsValidSegmentIndex(int32_t partIndex) const
{
    // 負数と配列末尾より後ろの番号を無効とする。
    if (partIndex < 0) {
        return false;
    }

    if (partIndex >= static_cast<int32_t>(segments_.size())) {
        return false;
    }

    return true;
}

float FearWormEnemy::GetHeadHpFraction() const
{
    if (segments_.empty()) {
        return 0.0f;
    }
    return (std::max)(0.0f, segments_[0].hp) / kHeadHp;
}

float FearWormEnemy::GetBodyHpFraction() const
{
    if (segments_.size() <= 1) {
        return 0.0f;
    }
    
    float totalMaxHp = (segments_.size() - 1) * kBodyHp;
    float currentTotalHp = 0.0f;
    
    for (size_t index = 1; index < segments_.size(); ++index) {
        if (segments_[index].isAlive) {
            currentTotalHp += (std::max)(0.0f, segments_[index].hp);
        }
    }
    
    return currentTotalHp / totalMaxHp;
}

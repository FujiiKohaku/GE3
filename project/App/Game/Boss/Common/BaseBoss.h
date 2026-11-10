#pragma once
#include "App/Game/Common/CombatTarget.h"
#include "App/Game/Common/EnemyBulletService.h"
#include "Engine/math/MathStruct.h"
#include "Engine/audio/SoundManager.h"
#if defined(_DEBUG) || defined(ENABLE_DEVELOPMENT_TOOLS)
#include "App/Game/Enemy/Debug/BaseEnemyDebugView.h"
#endif
#include <algorithm>
#include <cmath>
#include <memory>

class ShadowMapRenderer;
class DxrRenderer;

// 通常敵とは独立してボスのHP・死亡状態と攻撃対象の振る舞いを管理する。
class BaseBoss : public CombatTarget {
public:
    ~BaseBoss() override = default;
    virtual void Update() {}
    virtual void Draw() {}
    virtual void DrawShadow(ShadowMapRenderer&) {}
    virtual void SubmitRaytracing(DxrRenderer&) {}
    virtual void SetPosition(const Vector3& position) { transform_.translate = position; }
    Vector3 GetPosition() const override { return transform_.translate; }
    bool IsDead() const override { return isDead_; }
    virtual bool IsDeathSequenceFinished() const = 0;
    virtual bool IsMadModeActive() const = 0;
    virtual bool IsBeamHittingPlayer() const = 0;
    virtual float GetHeadHpFraction() const = 0;
    virtual float GetBodyHpFraction() const = 0;
    virtual bool RequiresBulletOcclusionCheck() const { return false; }
#if defined(_DEBUG) || defined(ENABLE_DEVELOPMENT_TOOLS)
    virtual std::unique_ptr<BaseEnemyDebugView> CreateDebugView() { return nullptr; }
#endif
    static std::unique_ptr<NormalEnemyBullet> AcquireNormalBullet(Model* model)
    {
        return EnemyBulletService::AcquireNormalBullet(model);
    }
    void SetDead(bool isDead)
    {
        if (isDead_ == isDead) { return; }
        isDead_ = isDead;
        if (isDead_) { OnDeath(); }
    }
    virtual void ApplyDamageToPart(int32_t, float damage)
    {
        if (isDead_ || !std::isfinite(damage) || damage <= 0.0f) { return; }
        SetHealth((std::max)(0.0f, hp_ - damage));
        if (hp_ <= 0.0f) { SetDead(true); }
    }
    void GetCollisionParts(std::vector<EnemyCollisionPart>& parts) const override
    {
        EnemyCollisionPart part {};
        part.position = GetPosition();
        parts.push_back(part);
    }
    bool IsCollisionPartDamageable(int32_t) const override { return true; }
    virtual void OnCollisionPartGuarded(int32_t, const Vector3&) {}
    SweepHit SweepBullet(const Sphere& bullet, const Vector3& movement, int32_t& partIndex) const override
    {
        if (IsDead()) { return {}; }
        std::vector<EnemyCollisionPart> parts;
        GetCollisionParts(parts);
        SweepHit nearest {};
        for (const auto& part : parts) {
            SweepHit hit = part.SweepSphere(bullet, movement);
            if (hit.isHit && (!nearest.isHit || hit.time < nearest.time)) {
                partIndex = part.partIndex;
                hit.position = part.position;
                nearest = hit;
            }
        }
        return nearest;
    }
    void OnBulletHit(int32_t partIndex, float damage, const Vector3& position) override
    {
        if (IsCollisionPartDamageable(partIndex)) { ApplyDamageToPart(partIndex, damage); }
        else { OnCollisionPartGuarded(partIndex, position); }
    }
    void RegisterRaycastTargets(CollisionObjectId& nextObjectId) const override
    {
        if (IsDead()) { return; }
        std::vector<EnemyCollisionPart> parts;
        GetCollisionParts(parts);
        for (const auto& part : parts) {
            if (part.isBox) {
                CollisionManager::GetInstance()->RegisterRaycastObbTarget(nextObjectId++, part.box);
            } else {
                CollisionManager::GetInstance()->RegisterRaycastSphereTarget(nextObjectId++, Sphere { part.position, part.radius });
            }
        }
    }
protected:
    virtual void OnDeath() {}
    void InitializeHealth(float hp)
    {
        if (!std::isfinite(hp) || hp <= 0.0f) { return; }
        hp_ = hp;
        isDead_ = false;
    }
    void SetHealth(float hp) { if (std::isfinite(hp) && hp >= 0.0f) { hp_ = hp; } }
    float GetHealth() const { return hp_; }
    void AddEnemyBullet(std::unique_ptr<EnemyBullet> bullet, bool shouldPlayShotSound = true)
    {
        EnemyBulletManager* bulletManager = EnemyBulletService::GetManager();
        if (bulletManager != nullptr) {
            if (shouldPlayShotSound) { SoundManager::GetInstance()->Play("EnemyShot"); }
            bulletManager->Add(std::move(bullet));
        }
    }
    EulerTransform transform_ {};
private:
    bool isDead_ = false;
    float hp_ = 1.0f;
};

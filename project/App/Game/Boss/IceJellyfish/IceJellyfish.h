#pragma once

#include "App/Game/Boss/StageBoss.h"
#include "Engine/CollisionManager/CollisionManager.h"
#include <array>
#include <memory>
#include <vector>

class Camera;
class Model;
class Player;

class IceJellyfish final : public StageBoss {
public:
    static constexpr float kMaxHp = 120.0f;
    static constexpr size_t kTentacleCount = 6;
    static constexpr size_t kSegmentsPerTentacle = 7;
    static constexpr size_t kIcePillarModelCount = 6;

    enum class AttackPattern {
        FrozenSweep,
        IcicleBloom,
        TentacleThrust,
        BlizzardCurrent,
        CrystalPrison,
        AbsoluteZero
    };

    void Initialize(Camera* camera, Model* bulletModel, Player* player);
    void Update() override;
    void Draw() override;
    void SetPosition(const Vector3& position) override;
    Vector3 GetPosition() const override { return basePosition_; }

    SweepHit SweepBullet(
        const Sphere& bullet, const Vector3& movement, int32_t& partIndex) const;
    void OnBulletHit(int32_t partIndex, float damage, const Vector3& position);
    void RegisterRaycastTargets(CollisionObjectId& nextObjectId) const;
    void DrawCollisionDebug() const;

    void GetCollisionParts(std::vector<EnemyCollisionPart>& parts) const override;
    bool IsCollisionPartDamageable(int32_t partIndex) const override;
    void ApplyDamageToPart(int32_t partIndex, float damage) override;
    void OnCollisionPartGuarded(int32_t partIndex, const Vector3& position) override;

    bool IsDeathSequenceFinished() const override { return deathTimer_ >= 2.0f; }
    bool IsMadModeActive() const override { return hp_ <= kMaxHp * 0.35f; }
    bool IsBeamHittingPlayer() const override { return false; }
    float GetHeadHpFraction() const override;
    float GetBodyHpFraction() const override;
    float GetHp() const { return hp_; }
    AttackPattern GetAttackPattern() const { return attackPattern_; }

private:
    static constexpr float kHoverDistance = 125.0f;
    static constexpr float kSegmentHp = 9.0f;
    static constexpr float kTentacleMaxHp = kSegmentHp * static_cast<float>(kSegmentsPerTentacle);
    static constexpr int32_t kCorePart = 0;
    static constexpr int32_t kFirstSegmentPart = 1;
    static constexpr int32_t kBellPart = 100;
    static constexpr int32_t kFirstCrystalPart = 200;

    std::unique_ptr<Object3d> CreatePart(
        Camera* camera, const char* modelPath, const Vector4& color, bool ice);
    void BeginPattern(AttackPattern pattern);
    void SelectNextPattern();
    void UpdatePattern(float deltaTime);
    void UpdateFrozenSweep();
    void UpdateIcicleBloom();
    void UpdateTentacleThrust();
    void UpdateBlizzardCurrent();
    void UpdateCrystalPrison();
    void UpdateAbsoluteZero();
    void FireAimedBurst(int count, float spread, float speed, int damage);
    void FireRadialBurst(int count, float speed, int damage, float angleOffset);
    void FirePulseRing(
        int count, float spread, float speed, float angleOffset, bool includeCenter);
    void UpdateAttackSequence(float floatPhase, float deltaTime);
    void StartIceSpearAttack();
    void FireIceSpearVolley();
    void StartIcePillarAttack();
    void PrepareIcePillarWarning();
    void UpdateIcePillarAttack(float deltaTime);
    void UpdatePartTransforms();
    void UpdateCrystalTransforms();
    void UpdateIcePillarModelTransforms();
    void DamagePlayerOnce(int slot, int damage);
    float PatternDuration() const;
    bool IsSegmentAlive(size_t tentacle, size_t segment) const;
    bool IsIceSpearTentacleSelected(size_t tentacle) const;
    bool IsIceSpearEmitter(size_t tentacle, size_t segment) const;
    Vector3 GetIceSpearAimTarget(size_t tentacle) const;
    int32_t SegmentPartIndex(size_t tentacle, size_t segment) const;

    std::unique_ptr<Object3d> bell_;
    std::unique_ptr<Object3d> core_;
    std::array<std::array<std::unique_ptr<Object3d>, kSegmentsPerTentacle>,
        kTentacleCount> tentacles_;
    std::array<std::unique_ptr<Object3d>, 4> crystals_;
    std::array<std::unique_ptr<Object3d>, kIcePillarModelCount> icePillarModels_;
    std::array<float, kTentacleCount> tentacleHp_ {};
    std::array<float, kTentacleCount> tentacleWavePhase_ {};
    std::array<float, kTentacleCount> tentacleWaveSpeed_ {};
    std::array<float, kTentacleCount> tentacleWaveAmplitude_ {};
    std::array<float, 4> crystalHp_ {};
    std::array<float, kTentacleCount> absoluteSealDamage_ {};
    std::array<bool, 8> attackHitApplied_ {};

    Camera* camera_ = nullptr;
    Model* bulletModel_ = nullptr;
    Player* player_ = nullptr;
    Vector3 basePosition_ {};
    Vector3 bodyPosition_ {};
    Vector3 lockedTarget_ {};
    float hoverCenterX_ = 0.0f;
    float hoverCenterY_ = 0.0f;
    float animationTime_ = 0.0f;
    float attackTimer_ = 0.0f;
    float previousAttackTimer_ = 0.0f;
    float hitFlashTime_ = 0.0f;
    float deathTimer_ = 0.0f;
    int32_t patternIndex_ = 0;
    int32_t phase_ = 1;
    int32_t firedWave_ = 0;
    bool coreExposed_ = false;
    bool pulseAttackArmed_ = false;
    int32_t pulseBarrageWave_ = -1;
    float pulseBarrageTimer_ = 0.0f;
    std::array<int32_t, 3> iceSpearTentacles_ {};
    int32_t iceSpearVolley_ = -1;
    float iceSpearTimer_ = 0.0f;
    float iceSpearPoseWeight_ = 0.0f;
    int32_t icePillarWave_ = -1;
    int32_t icePillarSafeLane_ = 1;
    float icePillarTimer_ = 0.0f;
    std::array<Vector3, 2> icePillarPositions_ {};
    std::array<Vector3, kIcePillarModelCount> icePillarModelPositions_ {};
    float icePillarModelTimer_ = 0.0f;
    bool icePillarModelSpawnedForWave_ = false;
    int32_t nextAttackIndex_ = 0;
    bool absoluteZeroUsed_ = false;
    bool absoluteInterrupted_ = false;
    AttackPattern attackPattern_ = AttackPattern::FrozenSweep;

    std::vector<OBB> bellColliders_;
    std::vector<OBB> tentacleColliders_;
    std::vector<int32_t> tentacleColliderParts_;
    std::array<Sphere, 4> crystalColliders_ {};
    Sphere coreCollider_ {};
};

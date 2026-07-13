#pragma once

#include "App/Game/Enemy/BaseEnemy.h"

class Model;
class Object3d;
class Player;

class FearWormEnemy : public BaseEnemy {
public:
    void Initialize(
        Model* model,
        Model* bulletModel,
        Player* player);

    void Update() override;
    void Draw() override;

    Vector3 GetPosition() const override;
    void SetPosition(const Vector3& position) override;

    void GetCollisionParts(std::vector<EnemyCollisionPart>& parts) const override;
    bool IsCollisionPartDamageable(int32_t partIndex) const override;
    void ApplyDamageToPart(int32_t partIndex, float damage) override;
    void OnCollisionPartGuarded(int32_t partIndex, const Vector3& position) override;

private:
    struct Segment {
        std::unique_ptr<Object3d> object;
        Vector3 position = { 0.0f, 0.0f, 0.0f };
        Vector3 scale = { 1.0f, 1.0f, 1.0f };
        float hp = 1.0f;
        float radius = 1.0f;
        float hitFlashTimer = 0.0f;
        bool isHead = false;
        bool isAlive = true;
    };

    void InitializeSegments(Model* model);
    void UpdateMovement();
    void UpdateSegments();
    void UpdateSegmentObjects();
    void UpdateBullets();
    void RemoveDeadBullets();
    Vector3 CalculateEntryStartPosition(const Vector3& playerPosition) const;
    Vector3 CalculateOrbitTargetPosition(const Vector3& playerPosition) const;
    void StartOrbitEntry(const Vector3& playerPosition);
    void ResetHeadTrail();
    void RecordHeadTrail();
    Vector3 SampleHeadTrail(float distanceFromHead) const;
    void Attack() override;
    void FireBullet(const Vector3& position);
    void PlayBodyBreakEffect(const Vector3& position);
    void PlayHeadGuardEffect(const Vector3& position);
    void PlayHeadVulnerableEffect(const Vector3& position);
    bool HasAliveBodyParts() const;
    bool IsValidSegmentIndex(int32_t partIndex) const;
    void OnDeath() override;

    Player* player_ = nullptr;
    Model* bulletModel_ = nullptr;

    std::vector<Segment> segments_;
    std::vector<Vector3> headTrail_;

    Vector3 startPosition_ = { 0.0f, 0.0f, 0.0f };

    float moveTime_ = 0.0f;
    float segmentSpacing_ = 5.0f;
    float bulletSpeed_ = 1.05f;
    float activationLeadDistance_ = 160.0f;
    float parallelForwardOffset_ = 36.0f;
    float headTrailSampleStep_ = 1.2f;
    float enterTimer_ = 0.0f;
    float enterDuration_ = 1.20f;
    float orbitAngle_ = 2.35f;

    int32_t fireTimer_ = 0;
    int32_t fireInterval_ = 22;
    int32_t fireSegmentIndex_ = 0;

    bool isParallelStarted_ = false;
    bool vulnerableEffectPlayed_ = false;
};

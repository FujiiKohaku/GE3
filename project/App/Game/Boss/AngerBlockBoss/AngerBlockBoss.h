#pragma once

#include "App/Game/Boss/StageBoss.h"
#include <memory>

class Model;
class Object3d;
class Player;

class AngerBlockBoss final : public StageBoss {
public:
    void Initialize(Model* model, Model* bulletModel, Player* player);
    void Update() override;
    void Draw() override;

    Vector3 GetPosition() const override { return bodyPosition_; }
    void SetPosition(const Vector3& position) override;
    void GetCollisionParts(std::vector<EnemyCollisionPart>& parts) const override;
    bool IsCollisionPartDamageable(int32_t partIndex) const override;
    void ApplyDamageToPart(int32_t partIndex, float damage) override;

    bool IsDeathSequenceFinished() const override { return deathTimer_ >= 2.0f; }
    bool IsMadModeActive() const override { return isMadMode_; }
    bool IsBeamHittingPlayer() const override { return false; }
    float GetHeadHpFraction() const override;
    float GetBodyHpFraction() const override;

private:
    void UpdatePartTransforms();
    void CheckHandHit(const Vector3& handPosition);
    void UpdateCoreBehavior(float deltaTime);
    void UpdateCoreRush();
    void UpdateCoreBurst();
    void UpdateCoreStrafe();
    void FireCoreBurst(bool spread);

    std::unique_ptr<Object3d> body_;
    std::unique_ptr<Object3d> core_;
    std::unique_ptr<Object3d> leftHand_;
    std::unique_ptr<Object3d> rightHand_;
    Player* player_ = nullptr;
    Model* bulletModel_ = nullptr;

    Vector3 bodyPosition_ {};
    Vector3 leftHandPosition_ {};
    Vector3 rightHandPosition_ {};
    float coreHp_ = 100.0f;
    float leftHandHp_ = 25.0f;
    float rightHandHp_ = 25.0f;
    float battleTime_ = 0.0f;
    float previousCycle_ = 0.0f;
    float deathTimer_ = 0.0f;
    bool isMadMode_ = false;
    bool leftWarning_ = false;
    bool rightWarning_ = false;
    Vector3 leftPunchTarget_ {};
    Vector3 rightPunchTarget_ {};
    Vector3 doublePunchTarget_ {};
    bool doublePunchTargetCaptured_ = false;
    float coreRushTimer_ = 0.0f;
    bool coreRushWarning_ = false;
    bool coreRushTargetCaptured_ = false;
    Vector3 coreRushStart_ {};
    Vector3 coreRushTarget_ {};
    int32_t corePattern_ = 0;
    int32_t coreShotWave_ = 0;
};

#pragma once

#include "App/Game/Enemy/BaseEnemy.h"
#include "BaseSwarmFormation.h"
#include <cstdint>
#include <memory>
#include <algorithm>

class Model;
class Player;

class SwarmGroupState {
public:
    explicit SwarmGroupState(int32_t totalCount = 0)
        : totalCount_((std::max)(0, totalCount)), activeCount_(totalCount_) {}
    void RecordRemoval(bool escaped)
    {
        if (activeCount_ <= 0) { return; }
        --activeCount_;
        if (escaped) { ++escapedCount_; } else { ++defeatedCount_; }
    }
    int32_t GetTotalCount() const { return totalCount_; }
    int32_t GetActiveCount() const { return activeCount_; }
    int32_t GetDefeatedCount() const { return defeatedCount_; }
    int32_t GetEscapedCount() const { return escapedCount_; }
private:
    int32_t totalCount_ = 0;
    int32_t activeCount_ = 0;
    int32_t defeatedCount_ = 0;
    int32_t escapedCount_ = 0;
};

class SwarmEnemy : public BaseEnemy {
public:
    std::string_view GetAppearanceId() const override { return "SwarmEnemy"; }
    void Initialize(
        Model* model,
        Model* bulletModel,
        Player* player,
        const std::shared_ptr<SwarmGroupState>& groupState,
        SwarmFormationType formationType,
        int32_t slotIndex,
        int32_t travelDirection);

    void Update() override;
    void SetAppearance(Model* model) override;
    void GetCollisionParts(std::vector<EnemyCollisionPart>& parts) const override;

private:
    void Move() override;
    void Attack() override;
    void OnDeath() override;
    void FireMovingBullet();
    Vector3 CalculateFormationOffset() const;

private:
    Player* player_ = nullptr;
    Model* bulletModel_ = nullptr;
    std::shared_ptr<SwarmGroupState> groupState_;
    const BaseSwarmFormation* formation_ = GetSwarmFormation(SwarmFormationType::Spiral).strategy;
    int32_t slotIndex_ = 0;
    int32_t travelDirection_ = 1;
    float moveTime_ = 0.0f;
    float crossingSpeed_ = 12.0f;
    float centerStartX_ = -38.0f;
    float baseHeight_ = 2.0f;
    float forwardDistance_ = 120.0f;
    float forwardPositionZ_ = 0.0f;
    float previousPlayerPositionZ_ = 0.0f;
    float attackTimer_ = 0.0f;
    float fadeTimer_ = 0.0f;
    int32_t shotsFired_ = 0;
    Vector4 baseColor_ = { 1.0f, 1.0f, 1.0f, 1.0f };
    bool isFading_ = false;
    bool escaped_ = false;
};

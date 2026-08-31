#pragma once

#include "App/Game/Enemy/BaseEnemy.h"
#include <cstdint>
#include <memory>

class Model;
class Player;

enum class SwarmFormationType {
    Spiral,
    Wall,
    Glyph,
    Diamond,
    Wave,
    Arrow,
};

struct SwarmGroupState {
    int32_t totalCount = 0;
    int32_t activeCount = 0;
    int32_t defeatedCount = 0;
    int32_t escapedCount = 0;
};

class SwarmEnemy : public BaseEnemy {
public:
    void Initialize(
        Model* model,
        Model* bulletModel,
        Player* player,
        const std::shared_ptr<SwarmGroupState>& groupState,
        SwarmFormationType formationType,
        int32_t slotIndex,
        int32_t travelDirection);

    void Update() override;
    void GetCollisionParts(std::vector<EnemyCollisionPart>& parts) const override;

private:
    void Move() override;
    void Attack() override;
    void OnDeath() override;
    void FireMovingBullet();
    Vector3 CalculateFormationOffset() const;
    Vector3 CalculateSpiralOffset() const;
    Vector3 CalculateWallOffset() const;
    Vector3 CalculateGlyphOffset() const;
    Vector3 CalculateDiamondOffset() const;
    Vector3 CalculateWaveOffset() const;
    Vector3 CalculateArrowOffset() const;

private:
    Player* player_ = nullptr;
    Model* bulletModel_ = nullptr;
    std::shared_ptr<SwarmGroupState> groupState_;
    SwarmFormationType formationType_ = SwarmFormationType::Spiral;
    int32_t slotIndex_ = 0;
    int32_t travelDirection_ = 1;
    float moveTime_ = 0.0f;
    float crossingSpeed_ = 12.0f;
    float centerStartX_ = -38.0f;
    float baseHeight_ = 2.0f;
    float forwardDistance_ = 72.0f;
    float attackTimer_ = 0.0f;
    float fadeTimer_ = 0.0f;
    int32_t shotsFired_ = 0;
    Vector4 baseColor_ = { 1.0f, 1.0f, 1.0f, 1.0f };
    bool isFading_ = false;
    bool escaped_ = false;
};

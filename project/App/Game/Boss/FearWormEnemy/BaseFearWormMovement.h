#pragma once
#include "Engine/math/MathStruct.h"

enum class FearWormMovementType { Orbit, Coil, Weave, Drift, Line, Spiral };

struct FearWormMovementContext {
    Vector3 playerPosition;
    float orbitAngle;
    float patternTime;
    float forwardOffset;
    float madModeTime;
    float spiralAngle;
};

struct FearWormMovementState {
    float& patternTime;
    float& lineTransitionTime;
};

class BaseFearWormMovement {
public:
    BaseFearWormMovement(FearWormMovementType type, FearWormMovementType next, bool automatic = true)
        : type_(type), next_(next), automatic_(automatic) {}
    virtual ~BaseFearWormMovement() = default;
    virtual Vector3 CalculateTarget(const FearWormMovementContext& context) const = 0;
    virtual void Enter(FearWormMovementState state) const { state.patternTime = 0.0f; }
    virtual void Exit(FearWormMovementState) const {}
    virtual float UpdateLineBlend(FearWormMovementState state, float dt) const {
        state.lineTransitionTime = 0.0f;
        return 0.0f;
    }
    FearWormMovementType GetType() const { return type_; }
    FearWormMovementType GetNext() const { return next_; }
    bool AdvancesAutomatically() const { return automatic_; }
private:
    FearWormMovementType type_;
    FearWormMovementType next_;
    bool automatic_;
};

const BaseFearWormMovement& GetFearWormMovement(FearWormMovementType type);

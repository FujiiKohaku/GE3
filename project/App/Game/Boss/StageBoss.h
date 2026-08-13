#pragma once

#include "App/Game/Enemy/BaseEnemy.h"

class StageBoss : public BaseEnemy {
public:
    ~StageBoss() override = default;

    virtual bool IsDeathSequenceFinished() const = 0;
    virtual bool IsMadModeActive() const = 0;
    virtual bool IsBeamHittingPlayer() const = 0;
    virtual float GetHeadHpFraction() const = 0;
    virtual float GetBodyHpFraction() const = 0;
};

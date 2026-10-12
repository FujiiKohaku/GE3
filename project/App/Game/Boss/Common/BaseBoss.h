#pragma once

#include "App/Game/Enemy/BaseEnemy.h"

class BaseBoss : public BaseEnemy {
public:
    ~BaseBoss() override = default;

    virtual bool IsDeathSequenceFinished() const = 0;
    virtual bool IsMadModeActive() const = 0;
    virtual bool IsBeamHittingPlayer() const = 0;
    virtual float GetHeadHpFraction() const = 0;
    virtual float GetBodyHpFraction() const = 0;
};

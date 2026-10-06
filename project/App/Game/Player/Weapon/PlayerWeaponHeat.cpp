#include "App/Game/Player/Weapon/PlayerWeaponHeat.h"
#include <algorithm>

void PlayerWeaponHeat::Update(float deltaTimeSeconds, bool isFireHeld)
{
    if (deltaTimeSeconds <= 0.0f) {
        return;
    }
    bool shouldCoolHeat = true;
    if (graceRemainingSeconds_ > 0.0f) {
        if (isFireHeld) {
            shouldCoolHeat = false;
            graceRemainingSeconds_ = (std::max)(0.0f,
                graceRemainingSeconds_ - deltaTimeSeconds);
            if (graceRemainingSeconds_ <= 0.0f) {
                isOverheated_ = true;
            }
        } else {
            graceRemainingSeconds_ = 0.0f;
        }
    }
    float coolingPerSecond = kCoolingPerSecond;
    if (isOverheated_) {
        coolingPerSecond = kOverheatCoolingPerSecond;
    }
    if (shouldCoolHeat) {
        heatRatio_ = (std::max)(0.0f,
            heatRatio_ - coolingPerSecond * deltaTimeSeconds);
    }
    if (isOverheated_ && heatRatio_ <= 0.0f) {
        isOverheated_ = false;
    }
}

void PlayerWeaponHeat::AddShotHeat(bool isMinigun)
{
    if (isOverheated_) {
        return;
    }
    float shotHeat = kNormalShotHeat;
    if (isMinigun) {
        shotHeat = kMinigunShotHeat;
    }
    heatRatio_ = (std::min)(1.0f, heatRatio_ + shotHeat);
    if (heatRatio_ >= 1.0f && graceRemainingSeconds_ <= 0.0f) {
        graceRemainingSeconds_ = kGraceDurationSeconds;
    }
}

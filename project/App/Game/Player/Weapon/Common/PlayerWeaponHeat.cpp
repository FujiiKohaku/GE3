// 通常弾とミニガンで共有する熱量・冷却・過熱猶予と、熱量に応じた威力を計算する。
#include "App/Game/Player/Weapon/Common/PlayerWeaponHeat.h"
#include <algorithm>
#include <cmath>

// 熱量が高いほど威力を増やす。発射時点の熱量でその弾の威力を決める。
int PlayerWeaponHeat::GetShotDamage(int baseDamage) const
{
    // 発射時の熱量で威力を確定する。0%で等倍、100%で2倍。
    return baseDamage <= 0 ? 0 : static_cast<int>(std::lround(baseDamage * (1.0f + heatRatio_)));
}

// 連射中の過熱猶予と、射撃停止中・過熱中の冷却を進める。
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

// 発射した武器に応じた熱量を加え、満タンになったときの猶予を準備する。
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

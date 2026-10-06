#pragma once

class PlayerWeaponHeat {
public:
    void Update(float deltaTimeSeconds, bool isFireHeld);
    void AddShotHeat(bool isMinigun);
    float GetHeatRatio() const { return heatRatio_; }
    bool IsOverheated() const { return isOverheated_; }

private:
    float heatRatio_ = 0.0f;
    bool isOverheated_ = false;
    float graceRemainingSeconds_ = 0.0f;
    static constexpr float kGraceDurationSeconds = 30.0f / 60.0f;
    static constexpr float kNormalShotHeat = 0.08f;
    static constexpr float kMinigunShotHeat = 0.04f;
    static constexpr float kCoolingPerSecond = 0.20f;
    static constexpr float kOverheatCoolingPerSecond = 0.25f;
};

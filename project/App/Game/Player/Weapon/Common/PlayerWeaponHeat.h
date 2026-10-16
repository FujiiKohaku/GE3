// 通常弾とミニガンで共有する熱量・冷却・過熱猶予と、熱量に応じた威力を計算する。
#pragma once

class PlayerWeaponHeat {
public:
    // 連射中の過熱猶予と、射撃停止中・過熱中の冷却を進める。
    void Update(float deltaTimeSeconds, bool isFireHeld);
    // 発射した武器に応じた熱量を加え、満タンになったときの猶予を準備する。
    void AddShotHeat(bool isMinigun);
    float GetHeatRatio() const { return heatRatio_; }
    bool IsOverheated() const { return isOverheated_; }
    // 熱量が高いほど威力を増やす。発射時点の熱量でその弾の威力を決める。
    int GetShotDamage(int baseDamage) const;

private:
    float heatRatio_ = 0.0f;
    bool isOverheated_ = false;
    float graceRemainingSeconds_ = 0.0f;
    static constexpr float kGraceDurationSeconds = 30.0f / 60.0f;
    static constexpr float kNormalShotHeat = 0.08f;
    static constexpr float kMinigunShotHeat = 0.025f;
    static constexpr float kCoolingPerSecond = 0.20f;
    static constexpr float kOverheatCoolingPerSecond = 0.25f;
};

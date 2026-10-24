// 生存しているロック対象へ進行方向を曲げ、目標を通り過ぎた回数も管理するミサイル。
#pragma once

#include "App/Game/Player/Combat/Bullet/Types/MissileBullet.h"
#include <memory>
#include <vector>

class BaseEnemy;

class HomingMissileBullet : public MissileBullet {
public:
    // ロック対象と候補一覧を記録する。候補一覧は弱参照で保持し、寿命を引き延ばさない。
    void SetTarget(BaseEnemy* target, const std::shared_ptr<const std::vector<BaseEnemy*>>& activeTargets);

protected:
    // 対象の生存を確認して進行方向を曲げ、接近時の補正と通過回数によって追尾を制御する。
    void Move() override;

private:
    BaseEnemy* target_ = nullptr;
    std::weak_ptr<const std::vector<BaseEnemy*>> activeTargets_;
    float homingStrength_ = 0.08f;
    int targetPassCount_ = 0;

    static constexpr float kCloseHomingDistance = 50.0f;
    static constexpr float kCloseHomingStrength = 0.32f;
    static constexpr float kTargetSnapDistance = 10.0f;
    static constexpr int kMaxTargetPassCount = 2;
};

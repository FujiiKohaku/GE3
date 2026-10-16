// 照準に近い敵をロックし、射撃ボタンを離したタイミングで発射を要求する。
#pragma once
#include "Engine/math/MathStruct.h"
#include <memory>
#include <vector>

class BaseEnemy;
class Camera;

class PlayerHomingLock {
public:
    // 現在の候補一覧を共有データへ反映し、弾が対象の生存を確認できるようにする。
    void SetTargets(const std::vector<BaseEnemy*>& targets);
    // 押下中のロック更新と、ボタンを離した瞬間の発射要求を判定する。
    bool UpdateFireInput(bool isFireHeld, bool isEnabled, Camera* camera,
        const Vector3& position, const Vector3& forward, const Vector2& aimPosition);
    void ClearTargets() { lockedTargets_.clear(); }
    // ロック対象と前回の押下状態を消し、次の射撃入力を新しく受け付ける。
    void Reset();
    // 生存中のロック対象の位置を集め、画面のロックマーカー表示に使う。
    void GetPositions(std::vector<Vector3>& positions) const;
    const std::vector<BaseEnemy*>& GetLockedTargets() const { return lockedTargets_; }
    std::shared_ptr<const std::vector<BaseEnemy*>> GetTargets() const { return targets_; }

private:
    // 前方距離と画面上の照準との近さを調べ、条件を満たす敵を上限数までロックする。
    void UpdateTargets(bool isLocking, Camera* camera, const Vector3& position,
        const Vector3& forward, const Vector2& aimPosition);
    std::shared_ptr<std::vector<BaseEnemy*>> targets_ = std::make_shared<std::vector<BaseEnemy*>>();
    std::vector<BaseEnemy*> lockedTargets_;
    bool wasFireHeld_ = false;
    static constexpr float kMaxForwardDistance = 250.0f;
    static constexpr size_t kMaxLockCount = 6;
};
